/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h>
#include "HwcUtils.h"
#include "gfxUtils.h"
#include "gfx2DGlue.h"
#include "mozilla/StaticPtr.h"

#define LOG_TAG "HwcUtils"

#if (LOG_NDEBUG == 0)
#define LOGD(args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, ## args)
#else
#define LOGD(args...) ((void)0)
#endif

#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ## args)


namespace mozilla {

/* Utility functions for HwcComposer */



/* static */ bool
HwcUtils::PrepareLayerRects(nsIntRect aVisible,
                            const gfx::Matrix& aLayerTransform,
                            const gfx::Matrix& aLayerBufferTransform,
                            nsIntRect aClip, nsIntRect aBufferRect,
                            bool aYFlipped,
                            hwc_rect_t* aSourceCrop, hwc_rect_t* aVisibleRegionScreen) {

    gfxMatrix aTransform = gfx::ThebesMatrix(aLayerTransform);
    gfxRect visibleRect(aVisible);
    gfxRect clip(aClip);
    gfxRect visibleRectScreen = aTransform.TransformBounds(visibleRect);
    // |clip| is guaranteed to be integer
    visibleRectScreen.IntersectRect(visibleRectScreen, clip);

    if (visibleRectScreen.IsEmpty()) {
        return false;
    }

    gfxMatrix inverse = gfx::ThebesMatrix(aLayerBufferTransform);
    inverse.Invert();
    gfxRect crop = inverse.TransformBounds(visibleRectScreen);

    //clip to buffer size
    crop.IntersectRect(crop, aBufferRect);
    crop.Round();

    if (crop.IsEmpty()) {
        return false;
    }

    //propagate buffer clipping back to visible rect
    gfxMatrix layerBufferTransform = gfx::ThebesMatrix(aLayerBufferTransform);
    visibleRectScreen = layerBufferTransform.TransformBounds(crop);
    visibleRectScreen.Round();

    // Map from layer space to buffer space
    crop -= aBufferRect.TopLeft();
    if (aYFlipped) {
        crop.y = aBufferRect.height - (crop.y + crop.height);
    }

    aSourceCrop->left = crop.x;
    aSourceCrop->top  = crop.y;
    aSourceCrop->right  = crop.x + crop.width;
    aSourceCrop->bottom = crop.y + crop.height;

    aVisibleRegionScreen->left = visibleRectScreen.x;
    aVisibleRegionScreen->top  = visibleRectScreen.y;
    aVisibleRegionScreen->right  = visibleRectScreen.x + visibleRectScreen.width;
    aVisibleRegionScreen->bottom = visibleRectScreen.y + visibleRectScreen.height;

    return true;
}

/* static */ bool
HwcUtils::PrepareVisibleRegion(const nsIntRegion& aVisible,
                               const gfx::Matrix& aLayerTransform,
                               const gfx::Matrix& aLayerBufferTransform,
                               nsIntRect aClip, nsIntRect aBufferRect,
                               RectVector* aVisibleRegionScreen) {

    gfxMatrix layerTransform = gfx::ThebesMatrix(aLayerTransform);
    gfxMatrix layerBufferTransform = gfx::ThebesMatrix(aLayerBufferTransform);
    gfxRect bufferRect = layerBufferTransform.TransformBounds(aBufferRect);
    nsIntRegionRectIterator rect(aVisible);
    bool isVisible = false;
    while (const nsIntRect* visibleRect = rect.Next()) {
        hwc_rect_t visibleRectScreen;
        gfxRect screenRect;

        screenRect = layerTransform.TransformBounds(gfxRect(*visibleRect));
        screenRect.IntersectRect(screenRect, bufferRect);
        screenRect.IntersectRect(screenRect, aClip);
        screenRect.Round();
        if (screenRect.IsEmpty()) {
            continue;
        }
        visibleRectScreen.left = screenRect.x;
        visibleRectScreen.top  = screenRect.y;
        visibleRectScreen.right  = screenRect.XMost();
        visibleRectScreen.bottom = screenRect.YMost();
        aVisibleRegionScreen->push_back(visibleRectScreen);
        isVisible = true;
    }

    return isVisible;
}

/* static */ bool
HwcUtils::CalculateClipRect(const gfx::Matrix& transform,
                            const nsIntRect* aLayerClip,
                            nsIntRect aParentClip, nsIntRect* aRenderClip) {

    gfxMatrix aTransform = gfx::ThebesMatrix(transform);
    *aRenderClip = aParentClip;

    if (!aLayerClip) {
        return true;
    }

    if (aLayerClip->IsEmpty()) {
        return false;
    }

    nsIntRect clip = *aLayerClip;

    gfxRect r(clip);
    gfxRect trClip = aTransform.TransformBounds(r);
    trClip.Round();
    gfxUtils::GfxRectToIntRect(trClip, &clip);

    aRenderClip->IntersectRect(*aRenderClip, clip);
    return true;
}


class ColorBuffer : public RefCounted<ColorBuffer>
{
  friend class ColorBufferFactory;

public:
  ColorBuffer(uint32_t w,uint32_t h,uint32_t color);
  virtual ~ColorBuffer();

  uint32_t GetWidth()
  {
    return mBuffer->getWidth();
  }

  uint32_t GetHeight()
  {
    return mBuffer->getHeight();
  }

  bool FillColor(uint32_t color);

  uint32_t GetColor()
  {
    return mColor;
  }

private:

  uint32_t mColor;
  android::sp<android::GraphicBuffer> mBuffer;
};

ColorBuffer::ColorBuffer(uint32_t w,uint32_t h,uint32_t color)
  : mColor(0)
{
  mBuffer=new android::GraphicBuffer(w, h,
                                     android::PIXEL_FORMAT_RGBA_8888,
                                     android::GraphicBuffer::USAGE_HW_COMPOSER |
                                     android::GraphicBuffer::USAGE_SW_READ_OFTEN |
                                     android::GraphicBuffer::USAGE_SW_WRITE_RARELY);

  FillColor(color);
}

ColorBuffer::~ColorBuffer()
{
}

bool ColorBuffer::FillColor(uint32_t color)
{
  if(!mBuffer.get()){
    return false;
  }

  if(mColor==color){
    return true;
  }

  uint32_t buffer_stride=mBuffer->getStride();
  uint32_t w=mBuffer->getWidth();
  uint32_t h=mBuffer->getHeight();

//  uint8_t *locked_buffer=nullptr;
//  uint32_t *pixel_buffer=nullptr;
//
//  //lock buffer
//  if(mBuffer->lock(android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
//                   android::GraphicBuffer::USAGE_SW_READ_NEVER, (void**)&locked_buffer)!=0){
//    return false;
//  }
//
//  //fill buffer
//  for(uint32_t i=0;i<h;++i){
//    pixel_buffer=reinterpret_cast<uint32_t*>(locked_buffer+buffer_stride*i);
//    memset(pixel_buffer,color,sizeof(uint32_t)*w);
//  }
//
//  //unlock buffer
//  if(mBuffer->unlock()!=0){
//    return false;
//  }
//
//  return true;

  void *locked_buffer=nullptr;
  uint8_t *byte_buffer=nullptr;
  uint32_t *pixel_buffer=nullptr;

  //lock buffer
  if(mBuffer->lock(android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                   android::GraphicBuffer::USAGE_SW_READ_NEVER, &locked_buffer)!=0){
    printf_stderr("bignose lock failed");
    return false;
  }

  //fill buffer
  int i,j;
  byte_buffer=reinterpret_cast<uint8_t*>(locked_buffer);
  for(i=0;i<h;++i){
    pixel_buffer=reinterpret_cast<uint32_t*>(byte_buffer+buffer_stride*i);
    for(j=0;j<w;++j){
      pixel_buffer[j]=color;
    }
  }

  //unlock buffer
  if(mBuffer->unlock()!=0){
    printf_stderr("bignose unlock failed");
    return false;
  }

  return true;
}

static ColorBufferFactory* sColorBufferFactoryInstance=nullptr;

ColorBufferFactory* ColorBufferFactory::GetSingleton(void)
{
  if(!sColorBufferFactoryInstance){
    sColorBufferFactoryInstance = new ColorBufferFactory();
  }

  return sColorBufferFactoryInstance;
}

ColorBufferFactory::ColorBufferFactory()
{
}

ColorBufferFactory::~ColorBufferFactory()
{
}

android::GraphicBuffer*
ColorBufferFactory::GetColorBuffer(uint32_t width,uint32_t height,uint32_t color)
{
  for(int i=0;i<mColorBuffers.size();++i){
    if(mColorBuffers[i].second==false){
      if(mColorBuffers[i].first->GetWidth()!=width || mColorBuffers[i].first->GetHeight()!=height) {
        mColorBuffers[i].first = new ColorBuffer(width,height,color);
        printf_stderr("bignose replace buffer:%d (%d,%d) => (%d,%d), color:%d",
            i,mColorBuffers[i].first->GetWidth(),mColorBuffers[i].first->GetHeight(),width,height,color);
      } else {
        mColorBuffers[i].first->FillColor(color);
        printf_stderr("bignose reuse buffer:%d (%d,%d), fill color:%d",
            i,mColorBuffers[i].first->GetWidth(),mColorBuffers[i].first->GetHeight(),color);
      }
      mColorBuffers[i].second=true;

      return mColorBuffers[i].first->mBuffer.get();
    }
  }

  mColorBuffers.push_back(std::pair<RefPtr<ColorBuffer>,bool>(new ColorBuffer(width,height,color),true));

  printf_stderr("bignose new buffer:(%d,%d) color:%d",
      mColorBuffers.back().first->GetWidth(),mColorBuffers.back().first->GetHeight(),color);

  return mColorBuffers.back().first->mBuffer.get();
}

void ColorBufferFactory::Reset(void)
{
  printf_stderr("bignose reset color layer buffer, size:%d",mColorBuffers.size());

  for(int i=0;i<mColorBuffers.size();++i){
    mColorBuffers[i].second=false;
  }
}

} // namespace mozilla
