/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LAYERS_SYNCOBJECT_H
#define MOZILLA_GFX_LAYERS_SYNCOBJECT_H

#include "mozilla/RefCounted.h"

struct ID3D11Device;

namespace mozilla {
namespace layers {

#ifdef XP_WIN
typedef void* SyncHandle;
#else
typedef uintptr_t SyncHandle;
#endif // XP_WIN

class RendererSyncObject : public RefCounted<RendererSyncObject>
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(RendererSyncObject)
  virtual ~RendererSyncObject() { }

  static already_AddRefed<RendererSyncObject> CreateRendererSyncObject(
#ifdef XP_WIN
                                                                       ID3D11Device* aDevice = nullptr
#endif
                                                                      );

  virtual bool Init() = 0;

  virtual SyncHandle GetSyncHandle() = 0;

  virtual bool Synchronize() = 0;

protected:
  RendererSyncObject() { }
};

class TextureSyncObject : public RefCounted<TextureSyncObject>
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(TextureSyncObject)
  virtual ~TextureSyncObject() { }

  static already_AddRefed<TextureSyncObject> CreateTextureSyncObject(SyncHandle aHandle
#ifdef XP_WIN
                                                                     , ID3D11Device* aDevice = nullptr
#endif
                                                                    );

  enum class SyncType {
    D3D11,
  };

  virtual SyncType GetSyncType() = 0;

  virtual void Synchronize() = 0;

  virtual bool IsSyncObjectValid() = 0;

protected:
  TextureSyncObject() { }
};

} // namespace layers
} // namespace mozilla

#endif //MOZILLA_GFX_LAYERS_SYNCOBJECT_H
