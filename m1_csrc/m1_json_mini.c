/* See COPYING.txt for license details. */

/*
 * m1_json_mini.c
 *
 * Minimal JSON parser for GitHub API responses.
 *
 * M1 Project
 */

#include "m1_json_mini.h"
#include <string.h>
#include <stdlib.h>

/* Skip whitespace characters */
static const char *skip_ws(const char *p)
{
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	return p;
}

/* Skip a JSON string starting at the opening quote.
 * Returns pointer past the closing quote, or NULL on error. */
static const char *skip_string(const char *p)
{
	if (*p != '"')
		return NULL;
	p++;
	while (*p && *p != '"')
	{
		if (*p == '\\')
		{
			p++; /* skip escaped char */
			if (!*p) return NULL;
		}
		p++;
	}
	if (*p == '"')
		return p + 1;
	return NULL;
}

/* Skip a JSON value (string, number, object, array, bool, null).
 * Returns pointer past the value, or NULL on error. */
static const char *skip_value(const char *p)
{
	p = skip_ws(p);
	if (!*p) return NULL;

	if (*p == '"')
		return skip_string(p);

	if (*p == '{')
	{
		/* Skip object: count braces */
		int depth = 1;
		p++;
		while (*p && depth > 0)
		{
			if (*p == '"')
			{
				p = skip_string(p);
				if (!p) return NULL;
				continue;
			}
			if (*p == '{') depth++;
			else if (*p == '}') depth--;
			p++;
		}
		return (depth == 0) ? p : NULL;
	}

	if (*p == '[')
	{
		/* Skip array: count brackets */
		int depth = 1;
		p++;
		while (*p && depth > 0)
		{
			if (*p == '"')
			{
				p = skip_string(p);
				if (!p) return NULL;
				continue;
			}
			if (*p == '[') depth++;
			else if (*p == ']') depth--;
			p++;
		}
		return (depth == 0) ? p : NULL;
	}

	/* Number, bool, null — skip until delimiter */
	while (*p && *p != ',' && *p != '}' && *p != ']'
		   && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
		p++;
	return p;
}

/*
 * Find "key" in JSON and return pointer to the value that follows the colon.
 * Only searches at the current nesting level (does not recurse into sub-objects).
 */
static const char *find_key_value(const char *json, const char *key)
{
	const char *p = json;
	size_t klen = strlen(key);

	while (*p)
	{
		/* Find next quote */
		p = strchr(p, '"');
		if (!p) return NULL;

		p++; /* past opening quote */

		/* Check if this key matches */
		if (strncmp(p, key, klen) == 0 && p[klen] == '"')
		{
			p += klen + 1; /* past key and closing quote */
			p = skip_ws(p);
			if (*p == ':')
			{
				p++;
				p = skip_ws(p);
				return p; /* pointer to value */
			}
		}
		else
		{
			/* Skip past this key string */
			while (*p && *p != '"')
			{
				if (*p == '\\') p++;
				p++;
			}
			if (*p == '"') p++;

			/* Skip colon + value */
			p = skip_ws(p);
			if (*p == ':')
			{
				p++;
				p = skip_value(p);
				if (!p) return NULL;
			}
		}
	}
	return NULL;
}

bool json_get_string(const char *json, const char *key, char *out_buf, uint16_t buf_size)
{
	const char *val;
	uint16_t i = 0;

	if (!json || !key || !out_buf || buf_size < 2)
		return false;

	out_buf[0] = '\0';
	val = find_key_value(json, key);
	if (!val || *val != '"')
		return false;

	val++; /* past opening quote */
	while (*val && *val != '"' && i < buf_size - 1)
	{
		if (*val == '\\')
		{
			val++;
			if (!*val) break;
		}
		out_buf[i++] = *val++;
	}
	out_buf[i] = '\0';
	return (i > 0);
}

bool json_get_int(const char *json, const char *key, int32_t *out_val)
{
	const char *val;
	char *end;

	if (!json || !key || !out_val)
		return false;

	val = find_key_value(json, key);
	if (!val) return false;

	/* Must start with digit or minus */
	if (*val != '-' && (*val < '0' || *val > '9'))
		return false;

	*out_val = (int32_t)strtol(val, &end, 10);
	return (end != val);
}

bool json_get_bool(const char *json, const char *key, bool *out_val)
{
	const char *val;

	if (!json || !key || !out_val)
		return false;

	val = find_key_value(json, key);
	if (!val) return false;

	if (strncmp(val, "true", 4) == 0)
	{
		*out_val = true;
		return true;
	}
	if (strncmp(val, "false", 5) == 0)
	{
		*out_val = false;
		return true;
	}
	return false;
}

const char *json_find_array(const char *json, const char *key)
{
	const char *val;

	if (!json || !key) return NULL;

	val = find_key_value(json, key);
	if (!val || *val != '[') return NULL;

	val++; /* past '[' */
	val = skip_ws(val);
	if (*val == ']') return NULL; /* empty array */
	return val;
}

const char *json_find_object(const char *json, const char *key)
{
	const char *val;

	if (!json || !key) return NULL;

	val = find_key_value(json, key);
	if (!val || *val != '{') return NULL;
	return val;
}

const char *json_array_next(const char *pos)
{
	if (!pos) return NULL;

	pos = skip_ws(pos);
	pos = skip_value(pos);
	if (!pos) return NULL;

	pos = skip_ws(pos);
	if (*pos == ',')
	{
		pos++;
		pos = skip_ws(pos);
		if (*pos == ']') return NULL; /* trailing comma */
		return pos;
	}
	return NULL; /* end of array or error */
}

const char *json_object_end(const char *pos)
{
	if (!pos || *pos != '{')
		return NULL;

	int depth = 1;
	pos++;
	while (*pos && depth > 0)
	{
		if (*pos == '"')
		{
			pos = skip_string(pos);
			if (!pos) return NULL;
			continue;
		}
		if (*pos == '{') depth++;
		else if (*pos == '}') depth--;
		pos++;
	}
	return (depth == 0) ? pos : NULL;
}
