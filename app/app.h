/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Copyright (C) 2026 Sam Oltz (VNL-1's changes) */
/* VNL-1: a generative lo-fi hip hop machine. This header is the app's half of the contract with the platform
 * (device/unity.c, host/plat_host.c); started from fm1-base's template. */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "VNL-1"
#define APP_SPLASH_RGB RGB(242, 120, 72)
#define APP_PALETTE 12u                  /* base/device/gfx.c's PALETTES: GARNET until Settings' THEME picks another */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */

/* The device's UI: 1 the screens' runtime (app/ui: the workbench's screens, generated;
   docs/superpowers/plans/2026-11-06-device-screens.md), 0 the old hand-drawn screen (app.c's draw()). The simulator
   has the runtime either way (its uiscreen command). */
#ifndef VNL_UI_RUNTIME
#define VNL_UI_RUNTIME 1
#endif

/* the store: the app's objects (the panel calibration the platform's), and their flash offsets. OBJ_SONGS, the saved
 * songs (app/ui/saved.c, 1348 B), has the pair that was Bluetooth's OBJ_BLE (number 2 at 0xDE000): VNL-1 has never
 * built the radio (fm1.toml ble = false), so nothing of its own was ever written there, and a record of another
 * firmware's reads as no songs (its magic). A build with the radio needs another pair for OBJ_BLE. A VNL-1 update
 * keeps it (the loader writes 0x4000..0x93000); stock V15 does not (its package's VM is 0x93000..0xE8FFF): the panel
 * pair and the songs go, OBJ_SETTINGS (0xFC000) stays. tools/flash.sh exports the songs around its stock step */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_SONGS, OBJ_NOBJ };

/* backup and restore over USB (fm1.toml backup = true; base/web/backup, tools/fm1_backup.py): the home and the saved
 * songs. A restore is refused while a song plays (STOP FIRST); it borrows the live state's buffer (main loop only, used
 * within a call) instead of 3840 B of the pool */
#define APP_BACKUP_OBJS {OBJ_SETTINGS, OBJ_SONGS}
#define APP_BACKUP_MAGIC 0x4B42534Eu     /* "NSBK" */
#define APP_BACKUP_VERSION 1
#define APP_BACKUP_WHAT "replaces saved songs"
#if VNL_UI_RUNTIME
#define APP_BACKUP_BUF live_buf
#endif
int app_backup_playing(void);            /* 1: no restore now (STOP FIRST) */
void app_backup_flush(void);             /* a backup starts: what waits for silence written (stopped) */
void app_backup_hold(int on);            /* no flash writes while a backup streams */
void app_backup_restored(void);          /* everything from the flash again */
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}

/* the sample bank (engine/samples.h) in two areas: PLAT_XDATA0 and the data area below the store's objects (one run:
 * its head and first part), then PLAT_XDATA1 (the rest); checked at boot, written over USB while stopped
 * (app_usb_cmd: tools/fm1_bank.py). The update never touches it (its loader writes 0x4000..0x93000). In the XDATA
 * windows' sectors bytes 0xF06..0xF07 never read "AT" (plat.h: the packer lays the bank out so, the write refuses
 * one that has it) */
#define APP_BANK_LO 0x93000u             /* PLAT_XDATA0_LO */
#define APP_BANK_HI 0xDC000u
#define APP_BANK1_LO 0xE5000u            /* PLAT_XDATA1_LO.._HI */
#define APP_BANK1_HI 0xFC000u
#ifndef PLAT_HOST
#define APP_OVERLOAD                     /* the platform: a block over 90 % of its time (the live state counts them) */
void app_overload(void);
#define APP_USB_CMD
void app_usb_cmd(const uint8_t *d, uint32_t n);
#endif

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the voices, and the first song rolled */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
#if VNL_UI_RUNTIME
#define APP_SPLASH
void app_splash(void);                   /* power-on: the splash (screens.js's), before the engine starts */
#endif
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"): playing (0/1), songs (started since boot), bar, section
 * (SEC_*, -1 none), tempo, key, pending (bars until the earliest knob change lands, -1 none), voices (sounding) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);   /* and remote (0/1: the browser has the controls) */
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
#define APP_HOST_CMD                     /* the browser's remote, as SysEx in and out (app.c) */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_FAILS
int app_host_fails(void);
#endif

/* SysEx from USB: the workbench's remote (F0 7D 56 4E; app.c) */
#define APP_SYSEX
int app_sysex(const uint8_t *p, uint32_t n);
