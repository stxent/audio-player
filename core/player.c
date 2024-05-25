/*
 * core/player.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifdef CONFIG_ENABLE_MP3
#  include "mp3common.h"
#  include "mp3dec.h"
#endif

#include "player.h"
#include "wav_defs.h"
#include <halm/wq.h>
#include <xcore/fs/utils.h>
#include <xcore/memory.h>
/*----------------------------------------------------------------------------*/
#define MAX_READ_RETRIES  4
#define MIN_BUFFER_LEVEL  64

enum TrackType
{
  TRACK_UNKNOWN,
  TRACK_WAV,
  TRACK_MP3
};
/*----------------------------------------------------------------------------*/
static void onAudioDataReceived(void *, struct StreamRequest *,
    enum StreamRequestStatus);
static void onAudioDataSent(void *, struct StreamRequest *,
    enum StreamRequestStatus);

static bool fetchNextChunkWAV(struct Player *, uint8_t *, size_t, size_t *);
static bool isDataAvailable(struct FsNode *);
static bool isFileSupported(const char *);
static bool isReservedName(const char *);
static void mockControlCallback(void *, uint32_t, uint8_t);
static void mockStateCallback(void *, enum PlayerState);
static struct FsNode *openTrack(struct Player *, size_t, struct TrackInfo *);
static void playTrack(struct Player *, size_t, int);
static bool parseHeaderWAV(struct Player *, struct FsNode *,
    struct TrackInfo *);
static void resetPlayback(struct Player *, struct FsNode *, size_t,
    const struct TrackInfo *);
static void scanNodeDescendants(struct Player *, struct FsNode *,
    const char *, unsigned int);
static void shuffleTracks(PathArray *, int (*)(void));
static void sortTracks(PathArray *);
static int trackCompare(const void *, const void *);

#ifdef CONFIG_ENABLE_MP3
static bool fetchNextChunkMP3(struct Player *, uint8_t *, size_t, size_t *);
static bool parseHeaderMP3(struct Player *, struct FsNode *,
    struct TrackInfo *);
#endif

static void abortPlayingTask(void *);
static void fetchNextChunkTask(void *);
static inline void playNextTask(void *);
static void stopPlayingTask(void *);
/*----------------------------------------------------------------------------*/
static void onAudioDataReceived(void *argument, struct StreamRequest *request,
    enum StreamRequestStatus status)
{
  // TODO
  (void)argument;
  (void)request;
  (void)status;
}
/*----------------------------------------------------------------------------*/
static void onAudioDataSent(void *argument, struct StreamRequest *request,
    enum StreamRequestStatus status)
{
  struct Player * const player = argument;

  request->length = 0;

  if (status != STREAM_REQUEST_COMPLETED || player->playback.stop)
  {
    wqAdd(WQ_DEFAULT, stopPlayingTask, player);
  }
  else if (player->playback.playing)
  {
    wqAdd(WQ_DEFAULT, fetchNextChunkTask, player);
  }
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_MP3
static bool fetchNextChunkMP3(struct Player *player, uint8_t *buffer,
    size_t capacity, size_t *count)
{
  struct TrackInfo * const info = &player->playback.info;
  size_t processed = 0;

  while (processed < capacity)
  {
    enum Result res = E_OK;

    if (!player->bufferSize)
    {
      /* Initial buffer filling */

      const FsLength offset = info->position % sizeof(player->buffer);
      size_t chunk = sizeof(player->buffer);

      /* Align the size of file read requests along file system sector size */
      if (offset != 0)
        chunk -= (size_t)offset;

      for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
      {
        res = fsNodeRead(
            player->playback.file,
            FS_NODE_DATA,
            info->position,
            player->buffer,
            chunk,
            &player->bufferSize
        );

        if (res == E_OK)
          break;
      }

      if (res == E_OK)
      {
        player->bufferPosition = 0;
        info->position += (FsLength)player->bufferSize;
      }
    }

    size_t left = player->bufferSize - player->bufferPosition;

    if (info->position < info->end && left <= sizeof(player->buffer) / 2)
    {
      /* Half of the buffer is available */

      size_t alignment = left & (sizeof(void *) - 1);
      size_t read;

      if (alignment)
        alignment = sizeof(void *) - alignment;
      if (alignment > player->bufferPosition)
        alignment = 0;

      player->bufferPosition -= alignment;
      left += alignment;

      if (player->bufferPosition > 0)
      {
        assert((left & (sizeof(void *) - 1)) == 0);
        memmove(player->buffer, player->buffer + player->bufferPosition, left);
      }

      for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
      {
        res = fsNodeRead(
            player->playback.file,
            FS_NODE_DATA,
            info->position,
            player->buffer + left,
            sizeof(player->buffer) / 2,
            &read
        );

        if (res == E_OK)
          break;
      }

      if (res == E_OK)
      {
        player->bufferPosition = alignment;
        player->bufferSize = left + read;
        info->position += (FsLength)read;
      }
    }

    if (res != E_OK)
      return false;

    if (player->bufferPosition >= player->bufferSize)
      break;

    const int offset = MP3FindSyncWord(player->buffer + player->bufferPosition,
        player->bufferSize - player->bufferPosition);

    if (offset >= 0)
    {
      player->bufferPosition += (size_t)offset;

      unsigned char *inputBuffer = player->buffer + player->bufferPosition;
      const int inputBufferSize = player->bufferSize - player->bufferPosition;
      int inputBytesLeft = inputBufferSize;

      const int error = MP3Decode(
          player->mp3Decoder,
          &inputBuffer,
          &inputBytesLeft,
          (short *)(buffer + processed),
          (capacity - processed) / sizeof(short),
          0 /* Normal MPEG format */
      );

      if (error == ERR_MP3_NONE)
      {
        MP3FrameInfo frameInfo;

        MP3GetLastFrameInfo(player->mp3Decoder, &frameInfo);
        processed += (size_t)frameInfo.outputSamps * sizeof(short);
      }
      else if (error == ERR_MP3_OUT_OF_MEMORY)
      {
        /* Output buffer is full */
        break;
      }

      if (error == ERR_MP3_INVALID_FRAMEHEADER)
      {
        /* Try from a next byte */
        ++player->bufferPosition;
      }
      else
      {
        player->bufferPosition += (size_t)(inputBufferSize - inputBytesLeft);
      }
    }
    else
    {
      /* Discard all buffered data and read a next chunk */
      player->bufferPosition = player->bufferSize;
    }
  }

  *count = processed;
  return true;
}
#endif
/*----------------------------------------------------------------------------*/
static bool fetchNextChunkWAV(struct Player *player, uint8_t *buffer,
    size_t capacity, size_t *count)
{
  struct TrackInfo * const info = &player->playback.info;
  size_t chunk = capacity;
  const FsLength left = info->end - info->position;
  const FsLength offset = info->position % chunk;
  enum Result res;

  /* Align the size of file read requests along file system sector size */
  if (offset != 0)
    chunk -= (size_t)offset;

  if (left < chunk)
    chunk = (size_t)left;

  for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
  {
    res = fsNodeRead(
        player->playback.file,
        FS_NODE_DATA,
        info->position,
        buffer,
        chunk,
        count
    );

    if (res == E_OK)
      break;
  }

  if (res == E_OK && *count == chunk)
  {
    info->position += (FsLength)(*count);
    return true;
  }
  else
    return false;
}
/*----------------------------------------------------------------------------*/
static bool isDataAvailable(struct FsNode *node)
{
  return fsNodeRead(node, FS_NODE_DATA, 0, NULL, 0, NULL) == E_OK;
}
/*----------------------------------------------------------------------------*/
static bool isFileSupported(const char *name)
{
  bool match = false;

  if (strstr(name, ".wav"))
  {
    match = true;
  }
#ifdef CONFIG_ENABLE_MP3
  else if (strstr(name, ".mp3"))
  {
    match = true;
  }
#endif
  else
  {
    match = false;
  }

  return match;
}
/*----------------------------------------------------------------------------*/
static bool isReservedName(const char *name)
{
  return name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]));
}
/*----------------------------------------------------------------------------*/
static void mockControlCallback(void *, uint32_t, uint8_t)
{
}
/*----------------------------------------------------------------------------*/
static void mockStateCallback(void *, enum PlayerState)
{
}
/*----------------------------------------------------------------------------*/
static struct FsNode *openTrack(struct Player *player, size_t position,
    struct TrackInfo *info)
{
  assert(player->handle != NULL);
  assert(position < pathArraySize(&player->tracks));

  struct FsNode * const node = fsOpenNode(player->handle,
      pathArrayAt(&player->tracks, position)->data);

  if (node != NULL)
  {
    if (parseHeaderWAV(player, node, info))
    {
      info->type = TRACK_WAV;
    }
#ifdef CONFIG_ENABLE_MP3
    else if (parseHeaderMP3(player, node, info))
    {
      info->type = TRACK_MP3;
    }
#endif
    else
    {
      info->type = TRACK_UNKNOWN;
    }
  }

  return node;
}
/*----------------------------------------------------------------------------*/
static void playTrack(struct Player *player, size_t start, int dir)
{
  const size_t count = pathArraySize(&player->tracks);

  if (player->handle == NULL || !count || start >= count)
    return;

  struct TrackInfo info;
  struct FsNode *node = NULL;
  size_t current = start;
  bool error = false;

  resetPlayback(player, NULL, 0, NULL);

  do
  {
    node = openTrack(player, current, &info);

    if (node != NULL)
    {
      if (info.type == TRACK_UNKNOWN)
      {
        fsNodeFree(node);
        node = NULL;

        if (dir > 0)
        {
          /* Try to play a next track */
          current = (current == count - 1) ? 0 : current + 1;
        }
        else
        {
          /* Try to play a previous track */
          current = (current == 0) ? count - 1 : current - 1;
        }
      }
      else
        break;
    }
    else
    {
      error = true;
      break;
    }
  }
  while (current != start);

  if (!error)
  {
    if (node != NULL)
    {
      resetPlayback(player, node, current, &info);
      wqAdd(WQ_DEFAULT, fetchNextChunkTask, player);

      player->stateCallback(player->stateCallbackArgument, PLAYER_PLAYING);
    }
    else
    {
      player->stateCallback(player->stateCallbackArgument, PLAYER_STOPPED);
    }
  }
  else
  {
    wqAdd(WQ_DEFAULT, abortPlayingTask, player);
  }
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_MP3
static bool parseHeaderMP3(struct Player *player, struct FsNode *node,
    struct TrackInfo *info)
{
  static const FsLength fileScanLength = 65536;
  FsLength headerPosition = 0;
  FsLength length;

  if (fsNodeLength(node, FS_NODE_DATA, &length) != E_OK || length == 0)
    return false;

  while (headerPosition < MIN(fileScanLength, length))
  {
    size_t count;
    enum Result res;

    for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
    {
      res = fsNodeRead(
          node,
          FS_NODE_DATA,
          headerPosition,
          player->buffer,
          sizeof(player->buffer),
          &count
      );

      if (res == E_OK)
        break;
    }

    if (res == E_OK && count == sizeof(player->buffer))
    {
      size_t bufferPosition = 0;

      while (bufferPosition < count)
      {
        const int offset = MP3FindSyncWord(player->buffer + bufferPosition,
            count - bufferPosition);

        if (offset >= 0)
        {
          MP3FrameInfo frameInfo;

          const int error = MP3GetNextFrameInfo(player->mp3Decoder, &frameInfo,
              player->buffer + offset, count - offset);

          if (error == ERR_MP3_NONE)
          {
            info->end = length;
            info->offset = headerPosition + (FsLength)offset;
            info->offset &= ~(sizeof(void *) - 1);
            info->position = info->offset;
            info->rate = (uint32_t)frameInfo.samprate;
            info->channels = (uint8_t)frameInfo.nChans;

            return true;
          }
          else
          {
            bufferPosition += (size_t)offset + 1;
          }
        }
        else
          break;
      }
    }

    headerPosition += (FsLength)count;
  }

  return false;
}
#endif
/*----------------------------------------------------------------------------*/
static bool parseHeaderWAV(struct Player *player, struct FsNode *node,
    struct TrackInfo *info)
{
  size_t count;
  enum Result res;

  for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
  {
    res = fsNodeRead(
        node,
        FS_NODE_DATA,
        0,
        player->buffer,
        sizeof(struct WavHeader),
        &count
    );

    if (res == E_OK)
      break;
  }

  if (res == E_OK && count == sizeof(struct WavHeader))
  {
    const struct WavHeader * const header =
        (const struct WavHeader *)player->buffer;
    const uint16_t channels = fromLittleEndian16(header->numChannels);
    const uint16_t width = header->bitsPerSample >> 3;

    if (fromBigEndian32(header->chunkId) != 0x52494646UL)
      return false;
    if (fromBigEndian32(header->subchunk1Id) != 0x666D7420UL)
      return false;
    if (fromBigEndian32(header->subchunk2Id) != 0x64617461UL)
      return false;
    if (fromLittleEndian16(header->audioFormat) != 1)
      return false;
    if (width != 2)
      return false;

    uint32_t alignment = channels * width;
    uint32_t duration = fromLittleEndian32(header->subchunk2Size);

    if (alignment < sizeof(void *))
      alignment = sizeof(void *);

    /* Align data size */
    duration &= ~(alignment - 1);

    info->end = sizeof(header) + duration;
    info->offset = sizeof(header);
    info->position = info->offset;
    info->rate = fromLittleEndian32(header->sampleRate);
    info->channels = (uint8_t)channels;

    return true;
  }

  return false;
}
/*----------------------------------------------------------------------------*/
static void resetPlayback(struct Player *player, struct FsNode *node,
    size_t index, const struct TrackInfo *info)
{
  if (player->playback.file != NULL)
    fsNodeFree(player->playback.file);

  player->bufferPosition = 0;
  player->bufferSize = 0;
  player->playback.stop = false;

  if (node != NULL)
  {
    player->playback.file = node;
    player->playback.index = index;
  }
  else
  {
    player->playback.file = NULL;
    player->playback.index = 0;
  }

  if (node != NULL && info != NULL)
  {
    player->playback.info = *info;
    player->playback.playing = true;

    player->controlCallback(player->controlCallbackArgument,
        info->rate, info->channels);
  }
  else
  {
    player->playback.info = (struct TrackInfo){
        .end = 0,
        .offset = 0,
        .position = 0,
        .rate = 0,
        .channels = 0,
        .type = TRACK_UNKNOWN
    };
    player->playback.playing = false;
  }
}
/*----------------------------------------------------------------------------*/
static void shuffleTracks(PathArray *tracks, int (*random)(void))
{
  const size_t count = pathArraySize(tracks);

  for (size_t i = count - 1; i; --i)
  {
    const size_t j = random() % (i + 1);
    FilePath swap;

    memcpy(&swap, pathArrayAt(tracks, i), sizeof(FilePath));
    memcpy(pathArrayAt(tracks, i), pathArrayAt(tracks, j), sizeof(FilePath));
    memcpy(pathArrayAt(tracks, j), &swap, sizeof(FilePath));
  }
}
/*----------------------------------------------------------------------------*/
static void sortTracks(PathArray *tracks)
{
  qsort(
      pathArrayAt(tracks, 0),
      pathArraySize(tracks),
      sizeof(FilePath),
      trackCompare
  );
}
/*----------------------------------------------------------------------------*/
static int trackCompare(const void *a, const void *b)
{
  const FilePath * const pathA = a;
  const FilePath * const pathB = b;

  return strcmp(pathA->data, pathB->data);
}
/*----------------------------------------------------------------------------*/
static inline void abortPlayingTask(void *argument)
{
  struct Player * const player = argument;

  resetPlayback(player, NULL, 0, NULL);
  player->stateCallback(player->stateCallbackArgument, PLAYER_ERROR);
}
/*----------------------------------------------------------------------------*/
static void fetchNextChunkTask(void *argument)
{
  struct Player * const player = argument;

  if (player->playback.file == NULL)
    return;

  for (size_t index = 0; index < player->buffers; ++index)
  {
    if (player->playback.info.position >= player->playback.info.end)
    {
      wqAdd(WQ_DEFAULT, playNextTask, player);
      break;
    }

    if (player->txReq[index].length == 0)
    {
      size_t count = 0;
      bool ok = false;

      switch ((enum TrackType)player->playback.info.type)
      {
        case TRACK_WAV:
          ok = fetchNextChunkWAV(player, player->txReq[index].buffer,
              player->txReq[index].capacity, &count);
          break;

#ifdef CONFIG_ENABLE_MP3
        case TRACK_MP3:
          ok = fetchNextChunkMP3(player, player->txReq[index].buffer,
              player->txReq[index].capacity, &count);
          break;
#endif

        default:
          break;
      }

      if (ok)
      {
        if (count >= MIN_BUFFER_LEVEL)
        {
          player->txReq[index].length = count;
          streamEnqueue(player->tx, &player->txReq[index]);
        }
        else
        {
          wqAdd(WQ_DEFAULT, playNextTask, player);
        }
      }
      else
      {
        wqAdd(WQ_DEFAULT, abortPlayingTask, player);
      }
    }
  }
}
/*----------------------------------------------------------------------------*/
static inline void playNextTask(void *argument)
{
  playerPlayNext(argument);
}
/*----------------------------------------------------------------------------*/
static inline void stopPlayingTask(void *argument)
{
  struct Player * const player = argument;

  if (player->playback.file != NULL)
  {
    player->bufferPosition = 0;
    player->bufferSize = 0;
    player->playback.info.position = player->playback.info.offset;
    player->playback.playing = false;
    player->playback.stop = false;
  }
  else
  {
    resetPlayback(player, NULL, 0, NULL);
  }

  player->stateCallback(player->stateCallbackArgument, PLAYER_STOPPED);
}
/*----------------------------------------------------------------------------*/
bool playerInit(struct Player *player, struct Stream *rx, struct Stream *tx,
    size_t buffers, size_t rxLength, size_t txLength, size_t trackCount,
    void *rxArena, void *txArena, void *trackArena, int (*random)(void))
{
  if (rxLength > txLength)
    return false;

  player->rxReq = malloc(sizeof(struct StreamRequest) * buffers);
  if (player->rxReq == NULL)
    return false;

  player->txReq = malloc(sizeof(struct StreamRequest) * buffers);
  if (player->txReq == NULL)
    goto free_rx;

  if (trackArena != NULL)
  {
    player->preallocated = true;
    pathArrayInitArena(&player->tracks, trackCount, trackArena);
  }
  else
  {
    player->preallocated = false;
    if (!pathArrayInit(&player->tracks, trackCount))
      goto free_tx;
  }

#ifdef CONFIG_ENABLE_MP3
  player->mp3Decoder = MP3InitDecoder();
  if (player->mp3Decoder == NULL)
    goto free_tracks;
#else
  player->mp3Decoder = NULL;
#endif

  player->controlCallback = mockControlCallback;
  player->controlCallbackArgument = NULL;
  player->stateCallback = mockStateCallback;
  player->stateCallbackArgument = NULL;
  player->random = random;
  player->shuffle = false;

  player->rx = rx;
  player->tx = tx;
  player->buffers = buffers;
  player->handle = NULL;

  player->playback.file = NULL;
  resetPlayback(player, NULL, 0, NULL);

  uint8_t *rxPosition = rxArena;
  uint8_t *txPosition = txArena;

  for (size_t index = 0; index < buffers; ++index)
  {
    player->rxReq[index] = (struct StreamRequest){
        rxLength,
        0,
        onAudioDataReceived,
        player,
        rxPosition
    };

    player->txReq[index] = (struct StreamRequest){
        txLength,
        0,
        onAudioDataSent,
        player,
        txPosition
    };

    rxPosition += rxLength;
    txPosition += txLength;
  }

  return true;

#ifdef CONFIG_ENABLE_MP3
free_tracks:
  if (!player->preallocated)
    pathArrayDeinit(&player->tracks);
#endif

free_tx:
  free(player->txReq);
free_rx:
  free(player->txReq);
  return false;
}
/*----------------------------------------------------------------------------*/
void playerDeinit(struct Player *player)
{
#ifdef CONFIG_ENABLE_MP3
  MP3FreeDecoder(player->mp3Decoder);
#endif

  if (!player->preallocated)
    pathArrayDeinit(&player->tracks);

  free(player->txReq);
  free(player->rxReq);
}
/*----------------------------------------------------------------------------*/
size_t playerGetCurrentTrack(const struct Player *player)
{
  return player->playback.index;
}
/*----------------------------------------------------------------------------*/
size_t playerGetTrackCount(const struct Player *player)
{
  return pathArraySize(&player->tracks);
}
/*----------------------------------------------------------------------------*/
void playerPlayNext(struct Player *player)
{
  /* Find a next track in the list */
  size_t next = player->playback.index + 1;

  if (next >= pathArraySize(&player->tracks))
    next = 0;

  playTrack(player, next, 1);
}
/*----------------------------------------------------------------------------*/
void playerPlayPause(struct Player *player)
{
  if (player->playback.file == NULL)
  {
    /* Playback was stopped, play from the beginning of the list */
    playTrack(player, 0, 1);
  }
  else
  {
    player->playback.playing = !player->playback.playing;

    if (player->playback.playing)
    {
      player->stateCallback(player->stateCallbackArgument, PLAYER_PLAYING);
      wqAdd(WQ_DEFAULT, fetchNextChunkTask, player);
    }
    else
    {
      player->stateCallback(player->stateCallbackArgument, PLAYER_PAUSED);
    }
  }
}
/*----------------------------------------------------------------------------*/
void playerPlayPrevious(struct Player *player)
{
  /* Find a previous track in the list */
  size_t current = player->playback.index;

  if (player->playback.index == 0)
    current = pathArraySize(&player->tracks);

  playTrack(player, current - 1, -1);
}
/*----------------------------------------------------------------------------*/
void playerResetFiles(struct Player *player)
{
  pathArrayClear(&player->tracks);
  resetPlayback(player, NULL, 0, NULL);
  player->handle = NULL;
}
/*----------------------------------------------------------------------------*/
static void scanNodeDescendants(struct Player *player, struct FsNode *root,
    const char *base, unsigned int level)
{
  static const unsigned int dirMaxLevel = 2;
  struct FsNode *child = NULL;

  for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
  {
    if ((child = fsNodeHead(root)) != NULL)
      break;
  }

  if (child != NULL)
  {
    enum Result res = E_OK;

    while (res == E_OK && !pathArrayFull(&player->tracks))
    {
      FilePath name = {{0}};
      FilePath path = {{0}};

      for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
      {
        res = fsNodeRead(child, FS_NODE_NAME, 0, name.data, sizeof(name),
            NULL);

        if (res == E_OK)
          break;
      }

      if (res == E_OK && !isReservedName(name.data))
      {
        const bool isAudioFile = isFileSupported(name.data);
        const bool isDataFile = isDataAvailable(child);

        fsJoinPaths(path.data, base, name.data);

        if (level < dirMaxLevel && !isDataFile)
        {
          /* Attention to stack size: recursive call */
          scanNodeDescendants(player, child, path.data, level + 1);
        }

        if (isDataFile && isAudioFile)
        {
          FsLength length;

          if (fsNodeLength(child, FS_NODE_DATA, &length) == E_OK && length > 0)
            pathArrayPushBack(&player->tracks, path);
        }
      }

      for (unsigned int retries = 0; retries < MAX_READ_RETRIES; ++retries)
      {
        res = fsNodeNext(child);

        if (res == E_OK || res == E_ENTRY)
          break;
      }
    }

    fsNodeFree(child);
  }
}
/*----------------------------------------------------------------------------*/
void playerScanFiles(struct Player *player, struct FsHandle *handle)
{
  assert(handle != NULL);

  pathArrayClear(&player->tracks);
  resetPlayback(player, NULL, 0, NULL);

  struct FsNode * const root = fsHandleRoot(handle);

  if (root != NULL)
  {
    player->handle = handle;

    scanNodeDescendants(player, root, "/", 0);
    fsNodeFree(root);

    if (!pathArrayEmpty(&player->tracks))
    {
      if (player->shuffle)
        shuffleTracks(&player->tracks, player->random);
      else
        sortTracks(&player->tracks);
    }
  }
  else
    player->handle = NULL;
}
/*----------------------------------------------------------------------------*/
void playerSetControlCallback(struct Player *player,
    void (*callback)(void *, uint32_t, uint8_t), void *argument)
{
  if (callback != NULL)
  {
    player->controlCallbackArgument = argument;
    player->controlCallback = callback;
  }
  else
  {
    player->controlCallbackArgument = NULL;
    player->controlCallback = mockControlCallback;
  }
}
/*----------------------------------------------------------------------------*/
void playerSetStateCallback(struct Player *player,
    void (*callback)(void *, enum PlayerState), void *argument)
{
  if (callback != NULL)
  {
    player->stateCallbackArgument = argument;
    player->stateCallback = callback;
  }
  else
  {
    player->stateCallbackArgument = NULL;
    player->stateCallback = mockStateCallback;
  }
}
/*----------------------------------------------------------------------------*/
void playerShuffleControl(struct Player *player, bool enable)
{
  assert(!enable || player->random != NULL);
  player->shuffle = enable;
}
/*----------------------------------------------------------------------------*/
void playerStopPlaying(struct Player *player)
{
  player->playback.stop = true;
}
