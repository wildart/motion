#ifndef _INCLUDE_OMX_H_
#define _INCLUDE_OMX_H_

#include <stdio.h>

#ifdef HAVE_OMX
#include "bcm_host.h"
#include "ilclient.h"
#endif /* HAVE_OMX */

struct omx {
#ifdef HAVE_OMX
    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
    OMX_PARAM_PORTDEFINITIONTYPE def;
    OMX_VIDEO_PARAM_BITRATETYPE bitrateType;
    FILE *output;

    int width;
    int height;

    void *udata;            /* U & V planes for greyscale images */
    int vbr;                /* variable bitrate setting */
#else
    int dummy;
#endif /* HAVE_OMX */
};

/* Initialize OMX stuff. Needs to be called before omx_open. */
void omx_init(void);

/* Shutdown OMX stuff. */
void omx_stop(void);

/*
 * Open an mpeg file. This is a generic interface for opening either an mpeg1 or
 * an mpeg4 video. If non-standard mpeg1 isn't supported (FFmpeg build > 4680),
 * calling this function with "mpeg1" as codec results in an error. To create a
 * timelapse video, use TIMELAPSE_CODEC as codec name.
 */
struct omx *omx_open(
    char *filename,
    unsigned char *y,    /* YUV420 Y plane */
    unsigned char *u,    /* YUV420 U plane */
    unsigned char *v,    /* YUV420 V plane */
    int width,
    int height,
    int rate,            /* framerate, fps */
    int bps,             /* bitrate; bits per second */
    int vbr              /* variable bitrate */
    );

/* Puts the image defined by u, y and v (YUV420 format). */
int omx_put_image(
    struct omx *omx,
    unsigned char *y,
    unsigned char *u,
    unsigned char *v
    );

/* Closes the mpeg file. */
void omx_close(struct omx *);

#endif /* _INCLUDE_OMX_H_ */