/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include <stddef.h>
#include <jni.h>

#define ART_LIBDEXFILE_DEX_UTF_INL_STANDALONE 1
#include "dex/utf-inl.h"

#include "nativehelper/ScopedUtfChars.h"
#include "nativehelper/scoped_string_chars.h"

/*
 * Template arguments:
 *    kUseShortZero: Encode U+0000 as a single byte with value 0 (otherwise emit 0xc0 0x80).
 *    kUse4ByteSequence: Encode valid surrogate pairs as a 4-byte sequence.
 *    kReplaceBadSurrogates: Replace unmatched surrogates with '?' (otherwise use 3-byte sequence).
 *                           Must be false if kUse4ByteSequence is false.
 *    Append: The type of the `append` functor. Should be deduced automatically.
 *
 * Encoding               kUseShortZero kUse4ByteSequence kReplaceBadSurrogates
 * UTF-8                  true          true              true
 * Modified UTF8          false         false             n/a
 * ART GetStringUTFChars  false         true              false
 */
#define ConvertUtf16ToArtUtf8            \
  art::ConvertUtf16ToUtf8<               \
      /*kUseShortZero=*/ false,          \
      /*kUse4ByteSequence=*/ true,       \
      /*kReplaceBadSurrogates=*/ false>

namespace nativehelper::utf {

char* ConvertJavaStringToArtUtf8(JNIEnv* env, jstring s) {
  if (s == nullptr) {
    jniThrowNullPointerException(env);
    return nullptr;
  }
  ScopedStringChars chars(env, s);
  size_t size = 0;
  ConvertUtf16ToArtUtf8(chars.get(), chars.size(), [&](char) { ++size; });
  char* utf_chars = static_cast<char*>(calloc(size + 1, 1));
  if (utf_chars == nullptr) {
    android::jnihelp::ThrowException(env, "java/lang/OutOfMemoryError", "()V");
    return nullptr;
  }
  char* p = utf_chars;
  ConvertUtf16ToArtUtf8(chars.get(), chars.size(), [&](char c) { *p++ = c; });
  return utf_chars;
}

}  // namespace nativehelper::utf
