/* See COPYING.txt for license details. */

/*
 * FLEX framing and page parsing adapted from Geofferey's Flipper Pager and
 * multimon-ng demod_flex.c (GNU GPL version 3 or later).
 * https://github.com/Geofferey/Flipper-Momentum-Apps/tree/flex/pocsag_pager
 * https://github.com/EliasOenal/multimon-ng
 */

#include "flex_receiver.h"

#include <stdio.h>
#include <string.h>

#define FLEX_SYNC_MARKER 0xA6C6AAAAUL
#define FLEX_SYNC_BAUD 1600U
#define FLEX_SYNC2_DURATION_MS 25U
#define FLEX_DATA_DURATION_MS 1760U
#define FLEX_MAX_RUN_SYMBOLS 512U
#define FLEX_NO_WORD 0xFFU
#define FLEX_NUMERIC_MAX_WORDS 9U
#define FLEX_FRAGMENT_TIMEOUT_MS (4U * 60U * 1000U)
#define FLEX_DUPLICATE_WINDOW_MS (5U * 60U * 1000U)
#define FLEX_GAP_MARK "[...]"
#define FLEX_TRUNCATION_MARK "..."

typedef enum {
    FLEX_PAGE_SECURE = 0,
    FLEX_PAGE_SHORT_INSTRUCTION,
    FLEX_PAGE_TONE,
    FLEX_PAGE_STANDARD_NUMERIC,
    FLEX_PAGE_SPECIAL_NUMERIC,
    FLEX_PAGE_ALPHANUMERIC,
    FLEX_PAGE_BINARY,
    FLEX_PAGE_NUMBERED_NUMERIC,
} flex_page_type_t;

typedef struct {
    uint16_t sync;
    uint16_t baud;
    uint8_t levels;
} flex_mode_t;

static const flex_mode_t flex_modes[] = {
    {0x870CU, 1600U, 2U},
    {0xB068U, 1600U, 4U},
    {0x7B18U, 3200U, 2U},
    {0xDEA0U, 3200U, 4U},
    {0x4C7CU, 3200U, 4U},
};

static uint8_t flex_popcount(uint32_t value)
{
    return (uint8_t)__builtin_popcount(value);
}

static uint16_t flex_bch_keys[31];
static bool flex_bch_initialized;

static void flex_bch_init(void)
{
    if (flex_bch_initialized)
        return;
    uint8_t exp[32];
    uint8_t element = 1U;
    for (uint8_t i = 0; i < 31U; ++i)
    {
        exp[i] = element;
        element <<= 1;
        if ((element & 0x20U) != 0U)
            element ^= 0x25U;
    }
    exp[31] = exp[0];
    for (uint8_t bit = 0; bit < 31U; ++bit)
    {
        const uint8_t index = 30U - bit;
        flex_bch_keys[bit] = ((uint16_t)exp[index] << 5) |
            exp[(3U * index) % 31U];
    }
    flex_bch_initialized = true;
}

static uint16_t flex_bch_syndrome(uint32_t codeword)
{
    flex_bch_init();
    uint16_t syndrome = 0;
    codeword &= 0x7FFFFFFFUL;
    while (codeword != 0U)
    {
        const uint8_t bit = (uint8_t)__builtin_ctz(codeword);
        syndrome ^= flex_bch_keys[bit];
        codeword &= codeword - 1U;
    }
    return syndrome;
}

static bool flex_bch_correct(uint32_t *word)
{
    const uint16_t syndrome = flex_bch_syndrome(*word);
    if (syndrome == 0U)
        return true;
    flex_bch_init();
    for (uint8_t first = 0; first < 31U; ++first)
    {
        if ((syndrome ^ flex_bch_keys[first]) == 0U)
        {
            *word ^= 1UL << first;
            return true;
        }
        for (uint8_t second = (uint8_t)(first + 1U); second < 31U; ++second)
        {
            if ((syndrome ^ flex_bch_keys[first] ^ flex_bch_keys[second]) == 0U)
            {
                *word ^= (1UL << first) | (1UL << second);
                return true;
            }
        }
    }
    return false;
}

static bool flex_checksum_valid(uint32_t word)
{
    uint8_t checksum = word & 0x0FU;
    checksum += (word >> 4) & 0x0FU;
    checksum += (word >> 8) & 0x0FU;
    checksum += (word >> 12) & 0x0FU;
    checksum += (word >> 16) & 0x0FU;
    checksum += (word >> 20) & 0x01U;
    return (checksum & 0x0FU) == 0x0FU;
}

static bool flex_alpha_checksum_valid(const uint32_t *words,
                                      uint8_t first, uint8_t last)
{
    uint32_t sum = 0;
    for (uint16_t i = first; i <= last; ++i)
    {
        uint32_t word = words[i] & 0x001FFFFFUL;
        if (i == first)
            word &= ~0x3FFUL;
        sum += (word & 0xFFU) + ((word >> 8) & 0xFFU) +
               ((word >> 16) & 0x1FU);
    }
    return ((~sum) & 0x3FFU) == (words[first] & 0x3FFU);
}

static bool flex_word_range_valid(const bool *valid, uint8_t first, uint8_t last)
{
    if (first >= FLEX_PHASE_WORDS || last >= FLEX_PHASE_WORDS || first > last)
        return false;
    for (uint8_t i = first; i <= last; ++i)
    {
        if (!valid[i])
            return false;
    }
    return true;
}

static void flex_clear_frame(flex_receiver_t *receiver)
{
    receiver->fiw_count = 0;
    receiver->fiw_raw = 0;
    receiver->sync2_count = 0;
    receiver->data_count = 0;
    receiver->data_bit_count = 0;
    receiver->phase_toggle = false;
}

static void flex_return_to_sync(flex_receiver_t *receiver)
{
    receiver->state = FLEX_RX_SYNC1;
    receiver->sync_buffer = 0;
    receiver->baud = FLEX_SYNC_BAUD;
    receiver->levels = 2;
    receiver->inverted = false;
    flex_clear_frame(receiver);
}

void flex_receiver_init(flex_receiver_t *receiver,
                        flex_receiver_message_fn callback,
                        void *callback_context)
{
    if (receiver == NULL)
        return;
    memset(receiver, 0, sizeof(*receiver));
    receiver->callback = callback;
    receiver->callback_context = callback_context;
    flex_return_to_sync(receiver);
}

void flex_receiver_reset(flex_receiver_t *receiver)
{
    if (receiver == NULL)
        return;
    flex_receiver_message_fn callback = receiver->callback;
    void *context = receiver->callback_context;
    memset(receiver, 0, sizeof(*receiver));
    receiver->callback = callback;
    receiver->callback_context = context;
    flex_return_to_sync(receiver);
}

static uint16_t flex_sync_check(uint64_t buffer)
{
    const uint32_t marker = (uint32_t)((buffer >> 16) & 0xFFFFFFFFULL);
    const uint16_t code_high = (uint16_t)(buffer >> 48);
    const uint16_t code_low = (uint16_t)~(uint16_t)buffer;
    if (flex_popcount(marker ^ FLEX_SYNC_MARKER) < 4U &&
        flex_popcount((uint16_t)(code_low ^ code_high)) < 4U)
    {
        return code_high;
    }
    return 0;
}

static bool flex_select_mode(flex_receiver_t *receiver, uint16_t sync_code)
{
    for (size_t i = 0; i < sizeof(flex_modes) / sizeof(flex_modes[0]); ++i)
    {
        if (flex_popcount((uint16_t)(flex_modes[i].sync ^ sync_code)) >= 4U)
            continue;
        if (flex_modes[i].levels == 4U && !receiver->symbol_input)
            return false;
        receiver->baud = flex_modes[i].baud;
        receiver->levels = flex_modes[i].levels;
        return true;
    }
    return false;
}

static bool flex_decode_fiw(flex_receiver_t *receiver)
{
    uint32_t fiw = receiver->fiw_raw;
    if (!flex_bch_correct(&fiw) || !flex_checksum_valid(fiw))
        return false;

    const uint8_t cycle = (fiw >> 4) & 0x0FU;
    if (cycle > 14U)
        return false;
    receiver->cycle = cycle;
    receiver->frame = (fiw >> 8) & 0x7FU;
    return true;
}

static void flex_start_text(flex_receiver_t *receiver)
{
    receiver->text_length = 0;
    receiver->message.text[0] = '\0';
}

static bool flex_append_character(flex_receiver_t *receiver, uint8_t character)
{
    if (character == 0x03U)
        return true;
    if (receiver->text_length + 1U >= PAGER_MESSAGE_TEXT_MAX)
        return false;
    if (character == '\r' || character == '\n' || character == '\t')
        character = ' ';
    if (character < 0x20U || character > 0x7EU)
        return true;
    receiver->message.text[receiver->text_length++] = (char)character;
    receiver->message.text[receiver->text_length] = '\0';
    return true;
}

static void flex_append_string(flex_receiver_t *receiver, const char *text)
{
    while (*text != '\0' && receiver->text_length + 1U < PAGER_MESSAGE_TEXT_MAX)
    {
        receiver->message.text[receiver->text_length++] = *text++;
    }
    receiver->message.text[receiver->text_length] = '\0';
}

static void flex_prepare_message(flex_receiver_t *receiver, uint32_t capcode,
                                 uint8_t type, char phase, uint8_t frame,
                                 bool partial)
{
    receiver->message.ric = capcode;
    receiver->message.baud = receiver->baud * (receiver->levels == 4U ? 2U : 1U);
    receiver->message.function = type;
    receiver->message.repeat_count = 1U;
    receiver->message.frequency_hz = 0U;
    receiver->message.protocol = PAGER_PROTOCOL_FLEX;
    receiver->message.levels = receiver->levels;
    receiver->message.cycle = receiver->cycle;
    receiver->message.frame = frame;
    receiver->message.phase = phase;
    receiver->message.partial = partial;
}

static uint32_t flex_text_hash(uint32_t capcode, uint8_t type, const char *text)
{
    size_t length = strlen(text);
    while (length > 0U && text[length - 1U] == ' ')
        --length;
    if (length > 0U && text[length - 1U] == ']')
    {
        size_t start = length - 1U;
        while (start > 0U && text[start - 1U] >= '0' && text[start - 1U] <= '9')
            --start;
        if (start < length - 1U && start > 0U && text[start - 1U] == '[')
        {
            length = start - 1U;
            while (length > 0U && text[length - 1U] == ' ')
                --length;
        }
    }

    uint32_t hash = 2166136261UL;
    hash = (hash ^ capcode) * 16777619UL;
    hash = (hash ^ type) * 16777619UL;
    for (size_t i = 0; i < length; ++i)
        hash = (hash ^ (uint8_t)text[i]) * 16777619UL;
    return hash | 1U;
}

static bool flex_seen_check(flex_receiver_t *receiver, uint32_t hash,
                            uint32_t capcode, bool partial)
{
    for (uint8_t i = 0; i < FLEX_SEEN_PAGES; ++i)
    {
        flex_seen_page_t *seen = &receiver->seen[i];
        if (seen->hash == hash && seen->capcode == capcode &&
            receiver->now_ms - seen->tick < FLEX_DUPLICATE_WINDOW_MS)
        {
            const bool was_partial = seen->partial;
            seen->tick = receiver->now_ms;
            if (!partial)
                seen->partial = false;
            return was_partial ? partial : true;
        }
    }

    flex_seen_page_t *seen = &receiver->seen[receiver->seen_next];
    seen->hash = hash;
    seen->capcode = capcode;
    seen->tick = receiver->now_ms;
    seen->partial = partial;
    receiver->seen_next = (receiver->seen_next + 1U) % FLEX_SEEN_PAGES;
    return false;
}

static void flex_emit_page(flex_receiver_t *receiver)
{
    const uint32_t hash = receiver->message.function == 6U &&
                          receiver->page_hash != 0U ?
        receiver->page_hash : flex_text_hash(receiver->message.ric,
                                             receiver->message.function,
                                             receiver->message.text);
    receiver->message.content_hash = hash;
    if (flex_seen_check(receiver, hash, receiver->message.ric,
                        receiver->message.partial))
        return;
    if (receiver->callback != NULL)
        receiver->callback(receiver->callback_context, &receiver->message);
}

static bool flex_parse_alpha(flex_receiver_t *receiver, const uint32_t *words,
                             const bool *valid, uint8_t first, uint8_t last)
{
    if (!flex_word_range_valid(valid, first, last) || first == last)
        return false;

    flex_start_text(receiver);
    receiver->alpha_signature = 0;
    receiver->alpha_signature_sum = 0;
    const uint8_t fragment = (words[first] >> 11) & 0x03U;
    ++first;
    bool truncated = false;
    for (uint8_t i = first; i <= last && !truncated; ++i)
    {
        const uint32_t word = words[i];
        if (i > first || fragment != 3U)
        {
            const uint8_t character = word & 0x7FU;
            if (character != 0x03U)
                receiver->alpha_signature_sum += character;
            truncated |= !flex_append_character(receiver, character);
        }
        else
        {
            receiver->alpha_signature = word & 0x7FU;
        }
        const uint8_t second = (word >> 7) & 0x7FU;
        const uint8_t third = (word >> 14) & 0x7FU;
        if (second != 0x03U)
            receiver->alpha_signature_sum += second;
        if (third != 0x03U)
            receiver->alpha_signature_sum += third;
        truncated |= !flex_append_character(receiver, second);
        truncated |= !flex_append_character(receiver, third);
    }
    if (receiver->text_length == 0U)
        return false;
    if (truncated)
        flex_append_string(receiver, FLEX_TRUNCATION_MARK);
    return true;
}

static bool flex_parse_numeric(flex_receiver_t *receiver, const uint32_t *words,
                               const bool *valid, uint8_t type, uint32_t vector,
                               uint8_t long_data_index)
{
    static const char flex_bcd[] = "0123456789 U -][";
    const uint8_t first = (vector >> 7) & 0x7FU;
    const uint16_t last = first + ((vector >> 14) & 0x07U);
    if ((long_data_index == FLEX_NO_WORD && last >= FLEX_PHASE_WORDS) ||
        (long_data_index != FLEX_NO_WORD && last > FLEX_PHASE_WORDS))
    {
        return false;
    }

    uint8_t order[FLEX_NUMERIC_MAX_WORDS];
    uint8_t count = 0;
    if (long_data_index != FLEX_NO_WORD)
    {
        order[count++] = long_data_index;
        for (uint8_t i = first; i < last; ++i)
            order[count++] = i;
    }
    else
    {
        for (uint8_t i = first; i <= last; ++i)
            order[count++] = i;
    }
    for (uint8_t i = 0; i < count; ++i)
    {
        if (!valid[order[i]])
            return false;
    }

    flex_start_text(receiver);
    uint8_t digit = 0;
    uint8_t remaining = type == FLEX_PAGE_NUMBERED_NUMERIC ? 14U : 6U;
    bool truncated = false;
    for (uint8_t i = 0; i < count && !truncated; ++i)
    {
        uint32_t word = words[order[i]];
        for (uint8_t bit = 0; bit < 21U; ++bit)
        {
            digit = (digit >> 1) & 0x0FU;
            if ((word & 1U) != 0U)
                digit ^= 0x08U;
            word >>= 1;
            if (--remaining != 0U)
                continue;
            if (digit != 0x0CU && !flex_append_character(receiver, flex_bcd[digit]))
                truncated = true;
            remaining = 4U;
        }
    }
    if (receiver->text_length == 0U)
        return false;
    if (truncated)
        flex_append_string(receiver, FLEX_TRUNCATION_MARK);
    return true;
}

static bool flex_parse_tone(flex_receiver_t *receiver, uint32_t vector,
                            const uint32_t *long_data)
{
    static const char flex_bcd[] = "0123456789 U -][";
    flex_start_text(receiver);
    if (((vector >> 7) & 0x03U) == 0U)
    {
        for (uint8_t shift = 9U; shift <= 17U; shift += 4U)
            flex_append_character(receiver, flex_bcd[(vector >> shift) & 0x0FU]);
        if (long_data != NULL)
        {
            for (uint8_t shift = 0; shift <= 16U; shift += 4U)
                flex_append_character(receiver,
                                      flex_bcd[(*long_data >> shift) & 0x0FU]);
        }
    }
    else
    {
        flex_append_string(receiver, "Tone only");
    }
    return true;
}

static uint32_t flex_binary_hash(uint32_t capcode, uint8_t type,
                                 const uint32_t *words, uint8_t first,
                                 uint8_t last)
{
    uint32_t hash = 2166136261UL;
    hash = (hash ^ capcode) * 16777619UL;
    hash = (hash ^ type) * 16777619UL;
    for (uint8_t i = first; i <= last; ++i)
    {
        for (uint8_t byte = 0; byte < 3U; ++byte)
            hash = (hash ^ ((words[i] >> (byte * 8U)) & 0xFFU)) * 16777619UL;
    }
    return hash | 1U;
}

static bool flex_parse_binary(flex_receiver_t *receiver, const uint32_t *words,
                              const bool *valid, uint8_t first, uint8_t last)
{
    if (!flex_word_range_valid(valid, first, last))
        return false;
    flex_start_text(receiver);
    receiver->page_truncated = false;
    receiver->page_hash = flex_binary_hash(receiver->message.ric,
                                           receiver->message.function,
                                           words, first, last);
    for (uint8_t i = first; i <= last; ++i)
    {
        const size_t needed = i == first ? 6U : 7U;
        if (receiver->text_length + needed >= PAGER_MESSAGE_TEXT_MAX)
        {
            receiver->page_truncated = true;
            flex_append_string(receiver, FLEX_TRUNCATION_MARK);
            break;
        }
        char encoded[10];
        (void)snprintf(encoded, sizeof(encoded), "%s%06lX",
                       i == first ? "" : " ", (unsigned long)words[i]);
        flex_append_string(receiver, encoded);
    }
    return true;
}

static void flex_gather_alpha(flex_receiver_t *receiver, const uint32_t *words,
                              const bool *valid, uint8_t first, uint8_t length,
                              uint8_t header_index)
{
    for (uint8_t i = 0; i < length; ++i)
    {
        const uint8_t source = header_index != FLEX_NO_WORD && i == 0U ?
            header_index : (header_index != FLEX_NO_WORD ? first + i - 1U : first + i);
        receiver->page_words[i] = words[source];
        receiver->page_valid[i] = valid[source];
    }
}

static void flex_fragment_append(flex_fragment_slot_t *slot, const char *text)
{
    const size_t capacity = PAGER_MESSAGE_TEXT_MAX - 1U;
    if (slot->text_length >= capacity)
        return;
    const size_t room = capacity - slot->text_length;
    const size_t length = strlen(text);
    if (length <= room)
    {
        memcpy(slot->text + slot->text_length, text, length + 1U);
        slot->text_length += length;
        return;
    }

    const size_t mark_length = sizeof(FLEX_TRUNCATION_MARK) - 1U;
    const size_t text_room = room > mark_length ? room - mark_length : 0U;
    if (text_room > 0U)
        memcpy(slot->text + slot->text_length, text, text_room);
    slot->text_length += text_room;
    const size_t mark_room = capacity - slot->text_length;
    memcpy(slot->text + slot->text_length, FLEX_TRUNCATION_MARK,
           mark_room < mark_length ? mark_room : mark_length);
    slot->text_length += mark_room < mark_length ? mark_room : mark_length;
    slot->text[slot->text_length] = '\0';
}

static void flex_fragment_flush(flex_receiver_t *receiver,
                                flex_fragment_slot_t *slot, bool incomplete)
{
    if (incomplete)
    {
        slot->suspect = true;
        flex_fragment_append(slot, FLEX_GAP_MARK);
    }
    flex_start_text(receiver);
    flex_append_string(receiver, slot->text);
    flex_prepare_message(receiver, slot->capcode, slot->type, slot->phase,
                         slot->frame, slot->suspect);
    flex_emit_page(receiver);
    memset(slot, 0, sizeof(*slot));
}

static void flex_fragment_flush_expired(flex_receiver_t *receiver)
{
    for (uint8_t i = 0; i < FLEX_FRAGMENT_SLOTS; ++i)
    {
        flex_fragment_slot_t *slot = &receiver->fragments[i];
        if (slot->used && receiver->now_ms - slot->tick >= FLEX_FRAGMENT_TIMEOUT_MS)
            flex_fragment_flush(receiver, slot, true);
    }
}

void flex_receiver_poll(flex_receiver_t *receiver, uint32_t now_ms)
{
    if (receiver == NULL)
        return;
    receiver->now_ms = now_ms;
    flex_fragment_flush_expired(receiver);
}

void flex_receiver_flush_fragments(flex_receiver_t *receiver)
{
    if (receiver == NULL)
        return;
    for (uint8_t i = 0; i < FLEX_FRAGMENT_SLOTS; ++i)
    {
        if (receiver->fragments[i].used)
            flex_fragment_flush(receiver, &receiver->fragments[i], true);
    }
}

static uint32_t flex_fragment_identity(uint32_t part_hash, size_t part_length,
                                       uint8_t fragment, bool continuation,
                                       uint8_t signature)
{
    uint32_t identity = part_hash ^ ((uint32_t)part_length * 0x9E3779B1UL);
    identity ^= (uint32_t)fragment << 29;
    identity ^= continuation ? 0xA5A5A5A5UL : 0x5A5A5A5AUL;
    if (fragment == 3U)
        identity ^= (uint32_t)signature * 0x01010101UL;
    return identity;
}

static bool flex_fragment_identity_seen(const flex_fragment_slot_t *slot,
                                        uint32_t identity)
{
    for (uint8_t i = 0; i < slot->identity_count; ++i)
    {
        if (slot->part_identities[i] == identity)
            return true;
    }
    return false;
}

static void flex_fragment_identity_remember(flex_fragment_slot_t *slot,
                                            uint32_t identity)
{
    slot->part_identities[slot->identity_next] = identity;
    slot->identity_next = (slot->identity_next + 1U) % FLEX_FRAGMENT_IDENTITIES;
    if (slot->identity_count < FLEX_FRAGMENT_IDENTITIES)
        ++slot->identity_count;
}

static void flex_fragment_add(flex_receiver_t *receiver, uint32_t capcode,
                              uint8_t type, uint8_t message_number,
                              uint8_t fragment, bool continuation,
                              char phase)
{
    flex_fragment_flush_expired(receiver);
    char part_text[PAGER_MESSAGE_TEXT_MAX];
    (void)snprintf(part_text, sizeof(part_text), "%s", receiver->message.text);
    const bool suspect = receiver->message.partial;
    flex_fragment_slot_t *slot = NULL;
    for (uint8_t i = 0; i < FLEX_FRAGMENT_SLOTS; ++i)
    {
        flex_fragment_slot_t *candidate = &receiver->fragments[i];
        if (candidate->used && candidate->capcode == capcode &&
            candidate->type == type && candidate->message_number == message_number)
        {
            slot = candidate;
            break;
        }
    }

    const uint32_t identity = flex_fragment_identity(
        flex_text_hash(capcode, type, part_text), strlen(part_text), fragment,
        continuation, receiver->alpha_signature);
    if (slot != NULL && fragment != slot->next_fragment &&
        flex_fragment_identity_seen(slot, identity))
    {
        slot->suspect |= suspect;
        slot->tick = receiver->now_ms;
        return;
    }

    if (slot == NULL && !continuation && fragment != 3U)
    {
        flex_prepare_message(receiver, capcode, type, phase, receiver->frame, true);
        flex_emit_page(receiver);
        return;
    }

    if (slot != NULL && fragment == 3U)
    {
        flex_fragment_flush(receiver, slot, true);
        slot = NULL;
    }

    if (slot == NULL)
    {
        slot = &receiver->fragments[0];
        for (uint8_t i = 0; i < FLEX_FRAGMENT_SLOTS; ++i)
        {
            flex_fragment_slot_t *candidate = &receiver->fragments[i];
            if (!candidate->used)
            {
                slot = candidate;
                break;
            }
            if ((int32_t)(candidate->tick - slot->tick) < 0)
                slot = candidate;
        }
        if (slot->used)
            flex_fragment_flush(receiver, slot, true);
        memset(slot, 0, sizeof(*slot));
        slot->used = true;
        slot->capcode = capcode;
        slot->type = type;
        slot->message_number = message_number;
        slot->suspect = suspect;
        slot->phase = phase;
        slot->frame = receiver->frame;
        if (fragment != 3U)
            flex_fragment_append(slot, FLEX_GAP_MARK);
    }
    else
    {
        slot->suspect |= suspect;
        if (fragment != slot->next_fragment)
            flex_fragment_append(slot, FLEX_GAP_MARK);
    }

    flex_fragment_append(slot, part_text);
    ++slot->parts;
    if (fragment == 3U)
    {
        slot->signature = receiver->alpha_signature;
        slot->signature_present = true;
    }
    slot->signature_sum += receiver->alpha_signature_sum;
    flex_fragment_identity_remember(slot, identity);
    slot->next_fragment = fragment == 3U ? 0U : (fragment + 1U) % 3U;
    slot->tick = receiver->now_ms;

    if (!continuation)
    {
        if (!slot->signature_present ||
            ((~slot->signature_sum) & 0x7FU) != slot->signature)
        {
            slot->suspect = true;
        }
        flex_fragment_flush(receiver, slot, false);
    }
}

static void flex_decode_phase(flex_receiver_t *receiver, uint32_t *words,
                              char phase)
{
    bool valid[FLEX_PHASE_WORDS];
    for (uint8_t i = 0; i < FLEX_PHASE_WORDS; ++i)
    {
        valid[i] = flex_bch_correct(&words[i]);
        if (valid[i])
            words[i] &= 0x001FFFFFUL;
    }
    if (!valid[0] || words[0] == 0U || words[0] == 0x001FFFFFUL)
        return;

    const uint32_t biw = words[0];
    const uint8_t vector_offset = (biw >> 10) & 0x3FU;
    const uint8_t address_offset = ((biw >> 8) & 0x03U) + 1U;
    if (vector_offset < address_offset ||
        (uint16_t)vector_offset + (uint16_t)(vector_offset - address_offset) >
            FLEX_PHASE_WORDS)
    {
        return;
    }

    for (uint8_t address_index = address_offset;
         address_index < vector_offset; ++address_index)
    {
        const uint8_t vector_index = vector_offset + address_index - address_offset;
        if (!valid[address_index] || !valid[vector_index])
            continue;
        const uint32_t address = words[address_index];
        if (address == 0U || address == 0x001FFFFFUL)
            continue;

        const bool long_address = address < 0x008001UL ||
            (address > 0x1E0000UL && address < 0x1F0001UL) ||
            address > 0x1F7FFEUL;
        uint32_t capcode;
        uint8_t long_data_index = FLEX_NO_WORD;
        if (long_address)
        {
            const uint8_t second = ++address_index;
            long_data_index = vector_index + 1U;
            if (second >= vector_offset || !valid[second] ||
                long_data_index >= FLEX_PHASE_WORDS)
            {
                continue;
            }
            const uint64_t value = (uint64_t)address +
                ((uint64_t)(words[second] ^ 0x001FFFFFUL) << 15) + 0x1F9000ULL;
            if (value > UINT32_MAX)
                continue;
            capcode = (uint32_t)value;
        }
        else
        {
            capcode = address - 0x8000UL;
        }

        const uint32_t vector = words[vector_index];
        if (!flex_checksum_valid(vector))
            continue;
        const uint8_t type = (vector >> 4) & 0x07U;
        if (type == FLEX_PAGE_SHORT_INSTRUCTION)
            continue;
        if (type == FLEX_PAGE_TONE)
        {
            const uint32_t *long_data = long_address && valid[long_data_index] ?
                &words[long_data_index] : NULL;
            if (flex_parse_tone(receiver, vector, long_data))
            {
                flex_prepare_message(receiver, capcode, type, phase,
                                     receiver->frame, false);
                flex_emit_page(receiver);
            }
            continue;
        }

        const bool numeric = type == FLEX_PAGE_STANDARD_NUMERIC ||
            type == FLEX_PAGE_SPECIAL_NUMERIC || type == FLEX_PAGE_NUMBERED_NUMERIC;
        const bool alpha = type == FLEX_PAGE_ALPHANUMERIC || type == FLEX_PAGE_SECURE;
        if (!numeric && !alpha && type != FLEX_PAGE_BINARY)
            continue;

        uint8_t first = (vector >> 7) & 0x7FU;
        const uint8_t length = numeric ? ((vector >> 14) & 0x07U) + 1U :
            (vector >> 14) & 0x7FU;
        const uint8_t field_words = (alpha || numeric) && long_address && length > 0U ?
            length - 1U : length;
        uint16_t last = first + field_words - 1U;
        if (length == 0U || length > FLEX_PHASE_WORDS ||
            (field_words > 0U && last >= FLEX_PHASE_WORDS))
        {
            continue;
        }

        const uint32_t *page_words = words;
        const bool *page_valid = valid;
        if (alpha)
        {
            flex_gather_alpha(receiver, words, valid, first, length,
                              long_address ? long_data_index : FLEX_NO_WORD);
            page_words = receiver->page_words;
            page_valid = receiver->page_valid;
            first = 0;
            last = length - 1U;
        }
        if (!flex_word_range_valid(page_valid, first, (uint8_t)last) &&
            !(numeric && long_address))
        {
            continue;
        }

        const bool checksum_valid = !alpha ||
            flex_alpha_checksum_valid(page_words, first, (uint8_t)last);
        receiver->page_hash = 0U;
        receiver->page_truncated = false;
        receiver->message.ric = capcode;
        receiver->message.function = type;
        bool parsed;
        if (numeric)
        {
            parsed = flex_parse_numeric(receiver, page_words, page_valid, type,
                                        vector, long_data_index);
        }
        else if (type == FLEX_PAGE_BINARY)
        {
            parsed = flex_parse_binary(receiver, page_words, page_valid, first,
                                       (uint8_t)last);
        }
        else
        {
            parsed = flex_parse_alpha(receiver, page_words, page_valid, first,
                                      (uint8_t)last);
        }
        if (!parsed)
            continue;

        flex_prepare_message(receiver, capcode, type, phase, receiver->frame,
                             !checksum_valid || receiver->page_truncated);
        if (alpha && checksum_valid)
        {
            const uint32_t header = page_words[first];
            const bool continuation = ((header >> 10) & 1U) != 0U;
            const uint8_t fragment = (header >> 11) & 0x03U;
            if (continuation || fragment != 3U)
            {
                flex_fragment_add(receiver, capcode, type,
                                  (header >> 13) & 0x3FU, fragment,
                                  continuation, phase);
                continue;
            }
        }
        flex_emit_page(receiver);
    }
}

static void flex_decode_data(flex_receiver_t *receiver)
{
    const uint16_t baud = receiver->baud;
    const uint8_t levels = receiver->levels;
    flex_decode_phase(receiver, receiver->phases[0], 'A');
    if (levels == 4U)
        flex_decode_phase(receiver, receiver->phases[1], 'B');
    if (baud == 3200U)
        flex_decode_phase(receiver, receiver->phases[2], 'C');
    if (baud == 3200U && levels == 4U)
        flex_decode_phase(receiver, receiver->phases[3], 'D');
    flex_fragment_flush_expired(receiver);
}

static void flex_read_data_symbol(flex_receiver_t *receiver, uint8_t symbol)
{
    const uint16_t index = ((receiver->data_bit_count >> 5) & 0xFFF8U) |
        (receiver->data_bit_count & 0x0007U);
    if (index >= FLEX_PHASE_WORDS)
        return;

    const bool bit_a = symbol > 1U;
    const bool bit_b = symbol == 1U || symbol == 2U;
    if (receiver->baud == 1600U || !receiver->phase_toggle)
    {
        receiver->phases[0][index] =
            (receiver->phases[0][index] >> 1) | (bit_a ? 0x80000000UL : 0U);
        if (receiver->levels == 4U)
        {
            receiver->phases[1][index] =
                (receiver->phases[1][index] >> 1) | (bit_b ? 0x80000000UL : 0U);
        }
        receiver->phase_toggle = receiver->baud == 3200U;
        if (receiver->baud == 1600U)
            ++receiver->data_bit_count;
    }
    else
    {
        receiver->phases[2][index] =
            (receiver->phases[2][index] >> 1) | (bit_a ? 0x80000000UL : 0U);
        if (receiver->levels == 4U)
        {
            receiver->phases[3][index] =
                (receiver->phases[3][index] >> 1) | (bit_b ? 0x80000000UL : 0U);
        }
        receiver->phase_toggle = false;
        ++receiver->data_bit_count;
    }
}

static void flex_process_symbol(flex_receiver_t *receiver, uint8_t symbol)
{
    if (receiver->state == FLEX_RX_SYNC1)
    {
        receiver->sync_buffer = (receiver->sync_buffer << 1) |
            (symbol < 2U ? 1U : 0U);
        uint16_t sync_code = flex_sync_check(receiver->sync_buffer);
        bool inverted = false;
        if (sync_code == 0U)
        {
            sync_code = flex_sync_check(~receiver->sync_buffer);
            inverted = sync_code != 0U;
        }
        if (sync_code != 0U && flex_select_mode(receiver, sync_code))
        {
            receiver->inverted = inverted;
            receiver->fiw_count = 0;
            receiver->fiw_raw = 0;
            receiver->state = FLEX_RX_FIW;
        }
        return;
    }

    const uint8_t rectified = receiver->inverted ? 3U - symbol : symbol;
    const bool bit = rectified > 1U;
    switch (receiver->state)
    {
        case FLEX_RX_FIW:
            ++receiver->fiw_count;
            if (receiver->fiw_count > 16U)
            {
                receiver->fiw_raw = (receiver->fiw_raw >> 1) |
                    (bit ? 0x80000000UL : 0U);
            }
            if (receiver->fiw_count == 48U)
            {
                if (flex_decode_fiw(receiver))
                {
                    receiver->sync2_count = 0;
                    receiver->state = FLEX_RX_SYNC2;
                }
                else
                {
                    flex_return_to_sync(receiver);
                }
            }
            break;

        case FLEX_RX_SYNC2:
            ++receiver->sync2_count;
            if (receiver->sync2_count >=
                (receiver->baud * FLEX_SYNC2_DURATION_MS) / 1000U)
            {
                receiver->data_count = 0;
                receiver->data_bit_count = 0;
                receiver->phase_toggle = false;
                memset(receiver->phases, 0, sizeof(receiver->phases));
                receiver->state = FLEX_RX_DATA;
            }
            break;

        case FLEX_RX_DATA:
            flex_read_data_symbol(receiver, rectified);
            ++receiver->data_count;
            if (receiver->data_count >=
                (receiver->baud * FLEX_DATA_DURATION_MS) / 1000U)
            {
                flex_decode_data(receiver);
                receiver->state = FLEX_RX_SYNC1;
                receiver->sync_buffer = 0;
                receiver->data_count = 0;
            }
            break;

        case FLEX_RX_SYNC1:
            break;
    }
}

void flex_receiver_feed(flex_receiver_t *receiver, bool level,
                        uint16_t duration_us, uint32_t now_ms)
{
    if (receiver == NULL || duration_us == 0U)
        return;
    receiver->now_ms = now_ms;
    receiver->symbol_input = false;
    int32_t remaining_half_us = (int32_t)duration_us * 2;
    uint16_t processed = 0;
    while (processed < FLEX_MAX_RUN_SYMBOLS)
    {
        const uint16_t symbol_period =
            receiver->state == FLEX_RX_SYNC1 || receiver->state == FLEX_RX_FIW ||
            receiver->baud == 1600U ? 1250U : 625U;
        if (remaining_half_us < (int32_t)(symbol_period / 2U))
            break;
        flex_process_symbol(receiver, level ? 3U : 0U);
        remaining_half_us -= symbol_period;
        ++processed;
    }

    const int32_t timing_error = remaining_half_us < 0 ?
        -remaining_half_us : remaining_half_us;
    const int32_t tolerance = receiver->state == FLEX_RX_SYNC1 ||
        receiver->state == FLEX_RX_FIW || receiver->baud == 1600U ? 200 : 120;
    if (processed == FLEX_MAX_RUN_SYMBOLS || timing_error > tolerance)
        flex_return_to_sync(receiver);
}

void flex_receiver_feed_symbol(flex_receiver_t *receiver, uint8_t symbol,
                               uint32_t now_ms)
{
    if (receiver == NULL || symbol > 3U)
        return;
    receiver->now_ms = now_ms;
    receiver->symbol_input = true;
    flex_process_symbol(receiver, symbol);
}
