#include "redismodule.h"

#include <magick/api.h>
#include <string.h>

static char gm_err_str[100];
static void GMErrorHandler(const ExceptionType severity, const char *reason, const char *description) {
  snprintf(gm_err_str, sizeof(gm_err_str), "GraphicsMagick error %d, %s: %s", severity, reason, description);
}

static void GMExceptionReply(RedisModuleCtx *ctx, ExceptionInfo *ex, const char* default_msg) {
  gm_err_str[0] = '\0';
  CatchException(ex);
  if (gm_err_str[0])
    RedisModule_ReplyWithError(ctx, gm_err_str);
  else if (default_msg) {
    snprintf(gm_err_str, sizeof(gm_err_str), "GraphicsMagick error %s", default_msg);
    RedisModule_ReplyWithError(ctx, gm_err_str);
  }
  else 
    RedisModule_ReplyWithError(ctx, "GraphicsMagick error");
}

typedef struct {
  RedisModuleKey *key;
  Image *org_img, *transformed_img;
  ImageInfo *ii;
  ExceptionInfo gm_exception;
} ImageTransforCtx;

static void CleanupImageTransformCtx(ImageTransforCtx *img_tr_ctx) {
  if (img_tr_ctx->ii)
    DestroyImageInfo(img_tr_ctx->ii);
  if (img_tr_ctx->org_img)
    DestroyImage(img_tr_ctx->org_img);
  if (img_tr_ctx->transformed_img)
    DestroyImage(img_tr_ctx->transformed_img);
    
  DestroyExceptionInfo(&img_tr_ctx->gm_exception);
  
  memset(img_tr_ctx, 0, sizeof(ImageTransforCtx));
}

static int GetImageToTransform(RedisModuleCtx *ctx, RedisModuleString *img_key, ImageTransforCtx *img_tr_ctx) {
  memset(img_tr_ctx, 0, sizeof(ImageTransforCtx)); // Init img_tr_ctx
  
  // open the key
  img_tr_ctx->key = RedisModule_OpenKey(ctx, img_key, REDISMODULE_READ | REDISMODULE_WRITE);
  
  // If key doesn't exist then return immediately
  if (RedisModule_KeyType(img_tr_ctx->key) == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_ERR; // We return REDISMODULE_ERR so caller knows there's no point in continuing
  }
  
  // Validate key is a string
  if (RedisModule_KeyType(img_tr_ctx->key) != REDISMODULE_KEYTYPE_STRING) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    return REDISMODULE_ERR;
  }
  
  // Generic GM setup
  img_tr_ctx->ii = CloneImageInfo(NULL);
  GetExceptionInfo(&img_tr_ctx->gm_exception);

  // Get access to the image
  size_t key_len;
  char *buf = RedisModule_StringDMA(img_tr_ctx->key, &key_len, REDISMODULE_READ);
  if (!buf) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    goto err;
  }
  
  // Read the source image
  img_tr_ctx->org_img = BlobToImage(img_tr_ctx->ii, buf, key_len, &img_tr_ctx->gm_exception);
  if (!img_tr_ctx->org_img) {
    GMExceptionReply(ctx, &img_tr_ctx->gm_exception, "bad input image");
    goto err;
  }

  return REDISMODULE_OK;

err:  
  // Cleanup on error
  CleanupImageTransformCtx(img_tr_ctx);

  return REDISMODULE_ERR;
}

static int UpdateTransformedImage(RedisModuleCtx *ctx, ImageTransforCtx *img_tr_ctx) {
  int res = REDISMODULE_ERR;
  
  // Get memory blob of transformed image
  size_t key_len;
  void *transformed_blob = ImageToBlob(img_tr_ctx->ii, img_tr_ctx->transformed_img, &key_len, &img_tr_ctx->gm_exception);
  if (!transformed_blob) {
    GMExceptionReply(ctx, &img_tr_ctx->gm_exception, NULL);
    goto err;
  }
  
  // Resize the key to contain the transformed image buffer
  if (RedisModule_StringTruncate(img_tr_ctx->key, key_len) == REDISMODULE_ERR) {
    RedisModule_ReplyWithError(ctx, "Error resizing string key");
    goto err;
  }
  
  // Get write access to the resized key
  char *buf = RedisModule_StringDMA(img_tr_ctx->key, &key_len, REDISMODULE_WRITE);
  if (!buf) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    goto err;
  }
  
  // Write transformed blob to output
  memcpy(buf, transformed_blob, key_len);

  // Setup success reply
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  res = REDISMODULE_OK;
  
err:
  CleanupImageTransformCtx(img_tr_ctx);
  return res;
}

int GMRotateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  
  // init auto memory for created strings
  RedisModule_AutoMemory(ctx);
  
  // Validate input is valid float
  double degrees;
  if (RedisModule_StringToDouble(argv[2], &degrees) == REDISMODULE_ERR) {
    RedisModule_ReplyWithError(ctx, "Invalid arguments");
    return REDISMODULE_ERR;
  }
  
  // Setup transformation context
  ImageTransforCtx img_tr_ctx;
  if (GetImageToTransform(ctx, argv[1], &img_tr_ctx) == REDISMODULE_ERR)
    return REDISMODULE_ERR;
  
  // Rotate original image
  img_tr_ctx.transformed_img = RotateImage(img_tr_ctx.org_img, degrees, &img_tr_ctx.gm_exception);
  if (!img_tr_ctx.transformed_img) {
    GMExceptionReply(ctx, &img_tr_ctx.gm_exception, NULL);
    goto err;
  }
  
  // Update key and finalize the transformation contex
  if (UpdateTransformedImage(ctx, &img_tr_ctx) == REDISMODULE_ERR)
    goto err;
    
  return REDISMODULE_OK;

err:  
  CleanupImageTransformCtx(&img_tr_ctx);
  return REDISMODULE_ERR;
}

int GMSwirlCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  
  // init auto memory for created strings
  RedisModule_AutoMemory(ctx);
  
  // Validate input is valid float
  double degrees;
  if (RedisModule_StringToDouble(argv[2], &degrees) == REDISMODULE_ERR) {
    RedisModule_ReplyWithError(ctx, "Invalid arguments");
    return REDISMODULE_ERR;
  }
  
  // Setup transformation context
  ImageTransforCtx img_tr_ctx;
  if (GetImageToTransform(ctx, argv[1], &img_tr_ctx) == REDISMODULE_ERR)
    return REDISMODULE_ERR;
  
  // Swirl it
  img_tr_ctx.transformed_img = SwirlImage(img_tr_ctx.org_img, degrees, &img_tr_ctx.gm_exception);
  if (!img_tr_ctx.transformed_img) {
    GMExceptionReply(ctx, &img_tr_ctx.gm_exception, NULL);
    goto err;
  }
  
  // Update key and finalize the transformation contex
  if (UpdateTransformedImage(ctx, &img_tr_ctx) == REDISMODULE_ERR)
    goto err;
    
  return REDISMODULE_OK;

err:  
  CleanupImageTransformCtx(&img_tr_ctx);
  return REDISMODULE_ERR;
}

int GMBlurCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  
  // init auto memory for created strings
  RedisModule_AutoMemory(ctx);
  
  // Validate inputs are valid floats
  double radius;
  if (RedisModule_StringToDouble(argv[2], &radius) == REDISMODULE_ERR) {
    RedisModule_ReplyWithError(ctx, "Invalid arguments");
    return REDISMODULE_ERR;
  }
  double sigma;
  if (RedisModule_StringToDouble(argv[3], &sigma) == REDISMODULE_ERR) {
    RedisModule_ReplyWithError(ctx, "Invalid arguments");
    return REDISMODULE_ERR;
  }
  
  // Setup transformation context
  ImageTransforCtx img_tr_ctx;
  if (GetImageToTransform(ctx, argv[1], &img_tr_ctx) == REDISMODULE_ERR)
    return REDISMODULE_ERR;
  
  // Blur it
  img_tr_ctx.transformed_img = BlurImage(img_tr_ctx.org_img, radius, sigma, &img_tr_ctx.gm_exception);
  if (!img_tr_ctx.transformed_img) {
    GMExceptionReply(ctx, &img_tr_ctx.gm_exception, NULL);
    goto err;
  }
  
  // Update key and finalize the transformation contex
  if (UpdateTransformedImage(ctx, &img_tr_ctx) == REDISMODULE_ERR)
    goto err;
    
  return REDISMODULE_OK;

err:  
  CleanupImageTransformCtx(&img_tr_ctx);
  return REDISMODULE_ERR;
}

int GMThumbnailCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  
  // init auto memory for created strings
  RedisModule_AutoMemory(ctx);
  
  // Validate inputs are valid unsigned ints
  long long width;
  if (RedisModule_StringToLongLong(argv[2], &width) == REDISMODULE_ERR || width <= 0) {
    RedisModule_ReplyWithError(ctx, "Invalid arguments");
    return REDISMODULE_ERR;
  }
  long long height;
  if (RedisModule_StringToLongLong(argv[3], &height) == REDISMODULE_ERR || height <= 0) {
    RedisModule_ReplyWithError(ctx, "Invalid arguments");
    return REDISMODULE_ERR;
  }
  
  // Setup transformation context
  ImageTransforCtx img_tr_ctx;
  if (GetImageToTransform(ctx, argv[1], &img_tr_ctx) == REDISMODULE_ERR)
    return REDISMODULE_ERR;
  
  // Generate thumbnail
  img_tr_ctx.transformed_img = ThumbnailImage(img_tr_ctx.org_img, (unsigned long)width, (unsigned long)height, &img_tr_ctx.gm_exception);
  if (!img_tr_ctx.transformed_img) {
    GMExceptionReply(ctx, &img_tr_ctx.gm_exception, NULL);
    goto err;
  }
  
  // Update key and finalize the transformation contex
  if (UpdateTransformedImage(ctx, &img_tr_ctx) == REDISMODULE_ERR)
    goto err;
    
  return REDISMODULE_OK;

err:  
  CleanupImageTransformCtx(&img_tr_ctx);
  return REDISMODULE_ERR;
}

int GMGetTypeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  int res = REDISMODULE_ERR;

  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  // init auto memory for created strings
  RedisModule_AutoMemory(ctx);

  RedisModuleKey *key;
  Image *img = NULL;
  ImageInfo *ii = NULL;
  ExceptionInfo gm_exception;

  // open the key
  key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

  // If key doesn't exist then return immediately
  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ReplyWithError(ctx, "empty key");
    return REDISMODULE_OK;
  }

  // Validate key is a string
  if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_STRING) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    return REDISMODULE_ERR;
  }

  // Generic GM setup
  ii = CloneImageInfo(NULL);
  GetExceptionInfo(&gm_exception);

  // Get access to the image
  size_t key_len;
  char *buf = RedisModule_StringDMA(key, &key_len, REDISMODULE_READ);
  if (!buf) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    goto err;
  }

  // Read the source image
  img = PingBlob(ii, buf, key_len, &gm_exception);
  if (!img) {
    GMExceptionReply(ctx, &gm_exception, "unknown image format");
    goto err;
  }

  RedisModule_ReplyWithSimpleString(ctx, img->magick);

  res = REDISMODULE_OK;

err:
  if (ii)
    DestroyImageInfo(ii);
  if (img)
    DestroyImage(img);
  DestroyExceptionInfo(&gm_exception);

  return res;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {

  // Register the module itself
  if (RedisModule_Init(ctx, "graphicsmagick", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  
  // Register the commands
  if (RedisModule_CreateCommand(ctx, "GRAPHICSMAGICK.ROTATE", GMRotateCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "GRAPHICSMAGICK.SWIRL", GMSwirlCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "GRAPHICSMAGICK.BLUR", GMBlurCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "GRAPHICSMAGICK.THUMBNAIL", GMThumbnailCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "GRAPHICSMAGICK.TYPE", GMGetTypeCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  // Do this last so if we fail earlier we don't need to cleanup
  InitializeMagick(NULL); //TODO: when should I call the de-init: DestroyMagick() ??
  SetErrorHandler(GMErrorHandler);
  SetFatalErrorHandler(GMErrorHandler);

  return REDISMODULE_OK;
}