#pragma once
#include <furi.h>
#include <furi_hal_gpio.h>
#include <pico.h>
#include <pico/time.h>

typedef struct FuriHalI2cBus FuriHalI2cBus;
typedef struct FuriHalI2cBusHandle FuriHalI2cBusHandle;

typedef enum {
    FuriHalI2cBusEventInit,
    FuriHalI2cBusEventDeinit,
    FuriHalI2cBusEventLock,
    FuriHalI2cBusEventUnlock,
    FuriHalI2cBusEventActivate,
    FuriHalI2cBusEventDeactivate,
} FuriHalI2cBusEvent;

typedef enum {
    FuriHalI2cBusSlaveEventStart,
    FuriHalI2cBusSlaveEventReceive,
    FuriHalI2cBusSlaveEventRequest,
    FuriHalI2cBusSlaveEventRepeatedStart,
    FuriHalI2cBusSlaveEventStop,
} FuriHalI2cBusSlaveEvent;

typedef void (*FuriHalI2cBusEventCallback)(FuriHalI2cBus* bus, FuriHalI2cBusEvent event);
typedef int (*FuriHalI2cBusWriteCallback)(void* instance, uint8_t addr, const uint8_t* src, size_t len, bool nostop, absolute_time_t until);
typedef int (*FuriHalI2cBusReadCallback)(void* instance, uint8_t addr, uint8_t* rxbuf, uint len, bool nostop, absolute_time_t until);

typedef enum {
    FuriHalI2cBusHandleEventActivate,
    FuriHalI2cBusHandleEventDeactivate,
} FuriHalI2cBusHandleEvent;

typedef void (*FuriHalI2cBusHandleEventCallback)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event);
typedef void (*FuriHalI2cBusSlaveCallback)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context);
typedef uint8_t (*FuriHalI2cBusSlaveWriteCallback)(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size);
typedef uint8_t (*FuriHalI2cBusSlaveReadCallback)(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size);
typedef void (*FuriHalI2cBusSlaveResetCallback)(const FuriHalI2cBusHandle* handle);

struct FuriHalI2cBusHandle {
    FuriHalI2cBus* bus;
    FuriHalI2cBusHandleEventCallback callback;
};

typedef struct {
    FuriHalI2cBusEventCallback event;
    union {
        struct {
            FuriHalI2cBusReadCallback read_blocking;
            FuriHalI2cBusWriteCallback write_blocking;
        } master;
        struct {
            FuriHalI2cBusSlaveWriteCallback write_blocking;
            FuriHalI2cBusSlaveReadCallback read_blocking;
            FuriHalI2cBusSlaveResetCallback bus_reset;
            FuriHalI2cBusSlaveCallback callback;
            void* context;
        } slave;
    };
} FuriHalI2cBusAPI;

typedef enum {
    FuriHalI2cModeMaster,
    FuriHalI2cModeSlave,
} FuriHalI2cMode;

struct FuriHalI2cBus {
    void* data;
    const char* name;
    const FuriHalI2cBusHandle* current_handle;
    const GpioPin* sda;
    const GpioPin* scl;
    const FuriHalI2cMode mode;
    FuriMutex* mutex;
    FuriHalI2cBusAPI api;
};
