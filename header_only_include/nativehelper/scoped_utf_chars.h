/*
 * Copyright (C) 2010 The Android Open Source Project
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

#pragma once

#include <stddef.h>
#include <string.h>

#include <jni.h>

#include "nativehelper_utils.h"

// Protect this with __has_include to cope with `stl: "none"` users.
#if __has_include(<string_view>)
#include <string_view>
#endif

// This header defines 2 classes to get chars from Java strings with various UTF-8 encodings:
// ScopedJniUtfChars and ScopedArtUtfChars.
//
// ScopedJniUtfChars is a simple wrapper around the JNI method GetStringUTFChars.
// ScopedArtUtfChars always gets chars with the same encoding as GetStringUTFChars
// on ART, even when running on a non-ART host JVM.
//
// The several UTF-8 encodings we may encounter:
//    UseShortZero: Encode U+0000 as a single byte with value 0 (otherwise emit 0xc0 0x80).
//    Use4ByteSequence: Use 4-byte UTF-8 sequences instead of 2x 3-byte surrogate pairs.
//    ReplaceBadSurrogates: Replace unmatched surrogates with '?' (otherwise use 3-byte sequence).
// -----------------------------------------------------------------------------
// Encoding         UseShortZero  Use4ByteSequence  ReplaceBadSurrogates
// -----------------------------------------------------------------------------
// Standard UTF-8   true          true              true
// Modified UTF-8   false         false             n/a
// ART UTF-8        false         true              false
// -----------------------------------------------------------------------------
//
// Behavior of the 2 classes under different JVM environments:
// -----------------------------------------------------------------------------
// Class Type                Encoding on ART     Encoding on non-ART JVM
// -----------------------------------------------------------------------------
// ScopedJniUtfChars         ART UTF-8           Modified UTF-8
// ScopedArtUtfChars         ART UTF-8           ART UTF-8
// -----------------------------------------------------------------------------
//
// By default, ScopedUtfChars is an alias of ScopedJniUtfChars.
//
// In certain use cases, we may want to use the same native codebase for Android platform to build
// native libraries running on non-ART host JVMs (e.g. Ravenwood, Robolectric, LayoutLib etc.).
// It is possible that we want to transparently emulate the behavior of ART for ScopedUtfChars.
// To do so, define NATIVE_HELPER_EMULATE_ART_UTF_CHARS to 1, which will make ScopedUtfChars
// an alias of ScopedArtUtfChars.
//
// ScopedUtfChars behavior under different NATIVE_HELPER_EMULATE_ART_UTF_CHARS values:
// -----------------------------------------------------------------------------
// EMULATE_ART_UTF_CHARS        Encoding on ART     Encoding on non-ART JVM
// -----------------------------------------------------------------------------
// 0 or undefined (default)     ART UTF-8           Modified UTF-8
// 1                            ART UTF-8           ART UTF-8
// -----------------------------------------------------------------------------
//
// Unlike GetStringUTFChars, we throw NullPointerException rather than abort if
// passed a null jstring, and c_str will return nullptr.
// This makes the correct idiom very simple:
//
//   ScopedUtfChars name(env, java_name);
//   if (name.c_str() == nullptr) {
//     return nullptr;
//   }
//
// Also consider using `GET_UTF_OR_RETURN`, a shorthand for the 4 lines above.

class ScopedJniUtfChars;

// When ART_BASE_ADDRESS is defined, this header is being used by ART itself, which can also
// be built on non-Android host platforms. So we need to check for ART_BASE_ADDRESS in addition
// to __ANDROID__ to detect whether we are running on ART to make sure ScopedArtUtfChars is only
// implemented on non-ART host JVMs.
#if defined(__ANDROID__) || defined(ART_BASE_ADDRESS)
#define NATIVE_HELPER_IS_BUILT_FOR_ART 1
#endif

#if NATIVE_HELPER_IS_BUILT_FOR_ART
using ScopedArtUtfChars = ScopedJniUtfChars;
#else
class ScopedArtUtfChars;
#endif

#if NATIVE_HELPER_EMULATE_ART_UTF_CHARS
using ScopedUtfChars = ScopedArtUtfChars;
#else
using ScopedUtfChars = ScopedJniUtfChars;
#endif

namespace internal {

class ScopedUtfCharsBase {
public:
  const char* c_str() const noexcept {
    return utf_chars_;
  }

  size_t size() const noexcept {
    return strlen(utf_chars_);
  }

  const char& operator[](size_t n) const noexcept {
    return utf_chars_[n];
  }

#if __has_include(<string_view>)
  operator std::string_view() const noexcept { return utf_chars_; }
#endif

protected:
  ScopedUtfCharsBase() noexcept = default;
  ~ScopedUtfCharsBase() noexcept = default;
  ScopedUtfCharsBase(const char* utf_chars) noexcept : utf_chars_(utf_chars) {}
  const char* utf_chars_;
};

}  // namespace internal

class ScopedJniUtfChars : public internal::ScopedUtfCharsBase {
 public:
  ScopedJniUtfChars(JNIEnv* env, jstring s) noexcept : env_(env), string_(s) {
    if (s == nullptr) {
      utf_chars_ = nullptr;
      jniThrowNullPointerException(env);
    } else {
      utf_chars_ = env->GetStringUTFChars(s, nullptr);
    }
  }

  ScopedJniUtfChars(ScopedJniUtfChars&& rhs) noexcept :
      ScopedUtfCharsBase(rhs.utf_chars_), env_(rhs.env_), string_(rhs.string_) {
    rhs.env_ = nullptr;
    rhs.string_ = nullptr;
    rhs.utf_chars_ = nullptr;
  }

  ~ScopedJniUtfChars() noexcept {
    release_string();
  }

  ScopedJniUtfChars& operator=(ScopedJniUtfChars&& rhs) noexcept {
    if (this != &rhs) {
      // Delete the currently owned UTF chars.
      release_string();

      // Move the rhs ScopedJniUtfChars and zero it out.
      env_ = rhs.env_;
      string_ = rhs.string_;
      utf_chars_ = rhs.utf_chars_;
      rhs.env_ = nullptr;
      rhs.string_ = nullptr;
      rhs.utf_chars_ = nullptr;
    }
    return *this;
  }

 private:
  void release_string() noexcept {
    if (utf_chars_) {
      env_->ReleaseStringUTFChars(string_, utf_chars_);
    }
  }

  JNIEnv* env_;
  jstring string_;

  DISALLOW_COPY_AND_ASSIGN(ScopedJniUtfChars);
};

#ifndef NATIVE_HELPER_IS_BUILT_FOR_ART

#include <stdlib.h>

namespace nativehelper::utf {
char* ConvertJavaStringToArtUtf8(JNIEnv* env, jstring s);
}  // namespace nativehelper::utf

class ScopedArtUtfChars : public internal::ScopedUtfCharsBase {
public:
  ScopedArtUtfChars(JNIEnv* env, jstring s) noexcept {
    utf_chars_ = nativehelper::utf::ConvertJavaStringToArtUtf8(env, s);
  }

  ScopedArtUtfChars(ScopedArtUtfChars&& rhs) noexcept : ScopedUtfCharsBase(rhs.utf_chars_) {
    rhs.utf_chars_ = nullptr;
  }

  ~ScopedArtUtfChars() noexcept {
    release_string();
  }

  ScopedArtUtfChars& operator=(ScopedArtUtfChars&& rhs) noexcept {
    if (this != &rhs) {
      // Delete the currently owned UTF chars.
      release_string();

      // Move the rhs ScopedArtUtfChars and zero it out.
      utf_chars_ = rhs.utf_chars_;
      rhs.utf_chars_ = nullptr;
    }
    return *this;
  }

private:
  void release_string() noexcept {
    free((char*) utf_chars_);
  }
  DISALLOW_COPY_AND_ASSIGN(ScopedArtUtfChars);
};

#endif  // NATIVE_HELPER_IS_BUILT_FOR_ART

#undef NATIVE_HELPER_IS_BUILT_FOR_ART
