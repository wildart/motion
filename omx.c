#ifdef HAVE_OMX

#include "omx.h"
#include "motion.h"


static ILCLIENT_T *ilclient;
static COMPONENT_T *video_encode = NULL;

void omx_def(OMX_PARAM_PORTDEFINITIONTYPE def)
{
    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO,
            "%s: Port %u: %s %u/%u %u %u %s,%s,%s %ux%u %ux%u @%u %u",
            def.nPortIndex,
            def.eDir == OMX_DirInput ? "in" : "out",
            def.nBufferCountActual,
            def.nBufferCountMin,
            def.nBufferSize,
            def.nBufferAlignment,
            def.bEnabled ? "enabled" : "disabled",
            def.bPopulated ? "populated" : "not pop.",
            def.bBuffersContiguous ? "contig." : "not cont.",
            def.format.video.nFrameWidth,
            def.format.video.nFrameHeight,
            def.format.video.nStride,
            def.format.video.nSliceHeight,
            def.format.video.xFramerate, def.format.video.eColorFormat);
}

/**
 * omx_cleanups
 *      Clean up ffmpeg struct if something was wrong.
 *
 * Returns
 *      Function returns nothing.
 */
void omx_cleanups(struct omx *omx)
{
    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "%s: get state %d", omx->state);
    if (omx->state == OMX_StateExecuting) {
        ilclient_disable_port_buffers(video_encode, 200, NULL, NULL, NULL);
        ilclient_disable_port_buffers(video_encode, 201, NULL, NULL, NULL);
        ilclient_change_component_state(video_encode, OMX_StateIdle);
        omx->state = OMX_StateIdle;
    }
    // if (omx->state == OMX_StateIdle) {
    //     ilclient_change_component_state(video_encode, OMX_StateLoaded);
    //     omx->state = OMX_StateLoaded;
    // }

    free(omx);
}

/**
 * omx_init
 *      Initializes for OMX.
 *
 * Returns
 *      Function returns nothing.
 */
void omx_init(void)
{
    OMX_ERRORTYPE r;

    bcm_host_init();

    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "%s: OMX encoder init");

    if ((ilclient = ilclient_init()) == NULL) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: ilclient_init() failed");
        return NULL;
    }

    r = OMX_Init();
    if (r != OMX_ErrorNone) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: OMX_Init() failed with %x!", r);
        ilclient_destroy(ilclient);
        return NULL;
    }

    // create video_encode
    r = ilclient_create_component(ilclient, &video_encode, "video_encode",
            ILCLIENT_DISABLE_ALL_PORTS |
            ILCLIENT_ENABLE_INPUT_BUFFERS |
            ILCLIENT_ENABLE_OUTPUT_BUFFERS);
    if (r != 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: ilclient_create_component() for video_encode failed with %x!", r);
        return NULL;
    }
}

void omx_stop(void)
{
    COMPONENT_T *list[5];
    memset(list, 0, sizeof(list));
    list[0] = video_encode;

    ilclient_cleanup_components(list);

    OMX_Deinit();

    ilclient_destroy(ilclient);
}

struct omx *omx_open(char *filename,
                     unsigned char *y, unsigned char *u, unsigned char *v,
                     int width, int height, int rate, int bps, int vbr)
{
    OMX_ERRORTYPE r;
    struct omx *omx;
    omx = mymalloc(sizeof(struct omx));
    memset(omx, 0, sizeof(struct omx));

    omx->vbr = vbr;
    omx->width = width;
    omx->height = height;

    // open output file
    strncat(filename, ".mp4", 4);
    omx->output = fopen(filename, "w");
    if (omx->output == NULL) {
        omx_cleanups(omx);
        return NULL;
    }

    // get current settings of video_encode component from port 200
    memset(&omx->def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    omx->def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    omx->def.nVersion.nVersion = OMX_VERSION;
    omx->def.nPortIndex = 200;

    if (OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexParamPortDefinition, &omx->def) != OMX_ErrorNone)
    {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO,
            "%s: %s:%d: OMX_GetParameter() for video_encode port 200 failed!",
            __FUNCTION__, __LINE__);
        omx_cleanups(omx);
        return NULL;
    }

    omx_def(omx->def);

    // set encoding resolution
    omx->def.format.video.nFrameWidth = width;
    omx->def.format.video.nFrameHeight = height;
    omx->def.format.video.xFramerate = rate << 16;
    omx->def.format.video.nSliceHeight = omx->def.format.video.nFrameHeight;
    omx->def.format.video.nStride = omx->def.format.video.nFrameWidth;
    omx->def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

    r = OMX_SetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexParamPortDefinition, &omx->def);
    if (r != OMX_ErrorNone) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO,
            "%s: %d: OMX_SetParameter() for video_encode port 200 failed with %x!", __LINE__, r);
        omx_cleanups(omx);
        return NULL;
    }

    omx_def(omx->def);

    // set format type
    memset(&omx->format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    omx->format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
    omx->format.nVersion.nVersion = OMX_VERSION;
    omx->format.nPortIndex = 201;
    omx->format.eCompressionFormat = OMX_VIDEO_CodingAVC;

    r = OMX_SetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexParamVideoPortFormat, &omx->format);
    if (r != OMX_ErrorNone) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO,
            "%s: %d: OMX_SetParameter() for video_encode port 201 failed with %x!", __LINE__, r);
        omx_cleanups(omx);
        return NULL;
    }

    // set current bitrate
    memset(&omx->bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
    omx->bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
    omx->bitrateType.nVersion.nVersion = OMX_VERSION;
    omx->bitrateType.eControlRate = OMX_Video_ControlRateVariable;
    omx->bitrateType.nTargetBitrate = bps;
    omx->bitrateType.nPortIndex = 201;
    r = OMX_SetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexParamVideoBitrate, &omx->bitrateType);
    if (r != OMX_ErrorNone) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO,
            "%s: %d: OMX_SetParameter() for bitrate for video_encode port 201 failed with %x!", __LINE__, r);
        omx_cleanups(omx);
        return NULL;
    }

    // Change state to IDLE
    if (ilclient_change_component_state(video_encode, OMX_StateIdle) == -1) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO,
            "%s: %d: ilclient_change_component_state(video_encode, OMX_StateIdle) failed", __LINE__);
    }
    omx->state = OMX_StateIdle;

    // Enable ports
    if (ilclient_enable_port_buffers(video_encode, 200, NULL, NULL, NULL) != 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: enabling port buffers for 200 failed!");
        omx_cleanups(omx);
        return NULL;
    }
    if (ilclient_enable_port_buffers(video_encode, 201, NULL, NULL, NULL) != 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: enabling port buffers for 201 failed!");
        omx_cleanups(omx);
        return NULL;
    }

    // Change state to EXECUTING
    ilclient_change_component_state(video_encode, OMX_StateExecuting);
    omx->state = OMX_StateExecuting;

    return omx;
}

/**
 * omx_prepare_frame
 *      Allocates and prepares a picture frame by setting up the U, Y and V pointers in
 *      the frame according to the passed pointers.
 */
int omx_prepare_frame(struct omx *omx, unsigned char *y,
                       unsigned char *u, unsigned char *v)
{
    int ret = 0;
    OMX_BUFFERHEADERTYPE *buf;

    // Allocate emptying buffer
    buf = ilclient_get_input_buffer(video_encode, 200, 1);
    if (buf == NULL) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: Could not alloc emptying buffer");
        return ret;
    }

    ret = (omx->width * omx->height * 3) / 2;
    memcpy(buf->pBuffer, y, ret);
    buf->nFilledLen  = ret;

    if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) != OMX_ErrorNone) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: Error emptying buffer!");
        return 0;
    }
    return ret;
}

/**
 * omx_put_image
 *      Puts an arbitrary picture defined by y, u and v.
 *
 * Returns
 *      Number of bytes written by omx_put_image
 *      -1 if any error happens in omx_put_image
 *       0 if error allocating picture.
 */
int omx_put_image(struct omx *omx, unsigned char *y,
                  unsigned char *u, unsigned char *v)
{
    int ret = 0;
    OMX_ERRORTYPE r;
    OMX_BUFFERHEADERTYPE *out;

    /* fill input buffer */
    ret = omx_prepare_frame(omx, y, u, v);
    if (ret) {
        // Allocate filling buffer
        out = ilclient_get_output_buffer(video_encode, 201, 1); // block
        if (out != NULL) {
            r = OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);
            if (r != OMX_ErrorNone) {
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: Error filling buffer: %x", r);
            }

            if (out != NULL) {
                ret = fwrite(out->pBuffer, 1, out->nFilledLen, omx->output);
                if (ret != out->nFilledLen) {
                   MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: fwrite: Error emptying buffer: %d!", ret);
                }
                MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, "%s: Saved frame: %d", ret);
                out->nFilledLen = 0;
            }
        }
    }
    return ret;
}

/**
 * omx_close
 *      Closes a video file.
 *
 * Returns
 *      Function returns nothing.
 */
void omx_close(struct omx *omx)
{
    if (omx->output != NULL){
        fclose(omx->output);
        MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, "%s: File closed");
    }
    omx_cleanups(omx);
}

#endif /* HAVE_OMX */