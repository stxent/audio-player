/*
 * core/player.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CORE_PLAYER_H_
#define CORE_PLAYER_H_
/*----------------------------------------------------------------------------*/
#include "wav_defs.h"
#include <xcore/containers/tg_array.h>
#include <xcore/fs/fs.h>
#include <xcore/stream.h>
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_PATH_LENGTH
#  define TRACK_PATH_LENGTH 64
#else
#  define TRACK_PATH_LENGTH CONFIG_PATH_LENGTH
#endif

enum [[gnu::packed]] PlayerState
{
  PLAYER_PLAYING,
  PLAYER_PAUSED,
  PLAYER_STOPPED,
  PLAYER_ERROR
};

typedef struct
{
  char data[TRACK_PATH_LENGTH];
} FilePath;

DEFINE_ARRAY(FilePath, Path, path)
/*----------------------------------------------------------------------------*/
struct TrackInfo
{
  /* End-of-file position in bytes */
  FsLength end;
  /* Offset to the audio data in bytes */
  FsLength offset;
  /* Position in bytes */
  FsLength position;
  /* Sample rate */
  uint32_t rate;
  /* Channel count */
  uint8_t channels;
  /* File type */
  uint8_t type;
};

struct Player
{
  void (*controlCallback)(void *, uint32_t, uint8_t);
  void *controlCallbackArgument;
  void (*stateCallback)(void *, enum PlayerState);
  void *stateCallbackArgument;

  struct Stream *rx;
  struct Stream *tx;
  struct StreamRequest *rxReq;
  struct StreamRequest *txReq;
  size_t buffers;

  struct FsHandle *handle;
  PathArray tracks;

  /* File buffer */
  union
  {
    uint8_t raw[4096];
    struct WavHeader wav;
  } buffer;

  /* Position from the beginning of the buffer in bytes */
  size_t bufferPosition;
  /* Bytes in the buffer */
  size_t bufferSize;

  struct
  {
    /* Current file */
    struct FsNode *file;
    /* File index in the track list */
    size_t index;

    /* Playing flag */
    bool playing;
    /* Stop playing request */
    bool stop;

    struct TrackInfo info;
  } playback;

  /* Helix MP3 decoder instance */
  void *mp3Decoder;
  /* Random number generation function */
  int (*random)(void);
  /* Track buffer is preallocated */
  bool preallocated;
  /* Enable track shuffle mode */
  bool shuffle;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

bool playerInit(struct Player *, struct Stream *, struct Stream *,
    size_t, size_t, size_t, size_t, void *, void *, void *, int (*)(void));
void playerDeinit(struct Player *);
size_t playerGetCurrentTrack(const struct Player *);
bool playerGetShuffleState(const struct Player *);
size_t playerGetTrackCount(const struct Player *);
const char *playerGetTrackName(struct Player *);
void playerPlayNext(struct Player *);
void playerPlayPause(struct Player *);
void playerPlayPrevious(struct Player *);
void playerResetFiles(struct Player *);
void playerScanFiles(struct Player *, struct FsHandle *);
void playerSetControlCallback(struct Player *,
    void (*)(void *, uint32_t, uint8_t), void *);
void playerSetStateCallback(struct Player *,
    void (*)(void *, enum PlayerState), void *);
void playerShuffleControl(struct Player *, bool);
void playerStopPlaying(struct Player *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* CORE_PLAYER_H_ */
