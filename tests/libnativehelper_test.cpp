/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <jni.h>

#include <memory>

#include "android-base/logging.h"
#include "gtest/gtest.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_utf_chars.h"
#include "nativehelper/utils.h"
#include "nativetesthelper_jni/utils.h"

class LibnativehelperTest : public ::testing::Test {
   protected:
    virtual void SetUp() override {
        int result = GetJavaVM()->GetEnv(reinterpret_cast<void**>(&mEnv), JNI_VERSION_1_6);
        CHECK_EQ(JNI_OK, result);
        CHECK_NE(nullptr, mEnv);
    }

    virtual void TearDown() override { mEnv = nullptr; }

    JNIEnv* mEnv;
};

TEST_F(LibnativehelperTest, GetUtfOrReturn) {
    ScopedLocalRef<jstring> j_str(mEnv, mEnv->NewStringUTF("foo"));
    std::unique_ptr<ScopedUtfChars> result;

    jint ret = [&](JNIEnv* env) -> jint {
        ScopedUtfChars str = GET_UTF_OR_RETURN(env, j_str.get());
        result.reset(new ScopedUtfChars(std::move(str)));
        return 1;
    }(mEnv);

    EXPECT_EQ(result->c_str(), std::string_view("foo"));
    EXPECT_FALSE(mEnv->ExceptionCheck());
    EXPECT_EQ(ret, 1);
}

TEST_F(LibnativehelperTest, GetUtfOrReturnVoid) {
    ScopedLocalRef<jstring> j_str(mEnv, mEnv->NewStringUTF("foo"));
    std::unique_ptr<ScopedUtfChars> result;

    [&](JNIEnv* env) -> void {
        ScopedUtfChars str = GET_UTF_OR_RETURN_VOID(env, j_str.get());
        result.reset(new ScopedUtfChars(std::move(str)));
    }(mEnv);

    EXPECT_EQ(result->c_str(), std::string_view("foo"));
    EXPECT_FALSE(mEnv->ExceptionCheck());
}

TEST_F(LibnativehelperTest, GetUtfOrReturnFailed) {
    jint ret = [&](JNIEnv* env) -> jint {
        ScopedUtfChars str = GET_UTF_OR_RETURN(env, nullptr);
        return 1;
    }(mEnv);

    EXPECT_TRUE(mEnv->ExceptionCheck());
    EXPECT_EQ(ret, 0);

    mEnv->ExceptionClear();
}

TEST_F(LibnativehelperTest, GetUtfOrReturnVoidFailed) {
    bool execution_completed = false;

    [&](JNIEnv* env) -> void {
        ScopedUtfChars str = GET_UTF_OR_RETURN_VOID(env, nullptr);
        execution_completed = true;
    }(mEnv);

    EXPECT_TRUE(mEnv->ExceptionCheck());
    EXPECT_FALSE(execution_completed);

    mEnv->ExceptionClear();
}

TEST_F(LibnativehelperTest, CreateUtfOrReturn) {
    std::unique_ptr<ScopedLocalRef<jstring>> result;

    jint ret = [&](JNIEnv* env) -> jint {
        ScopedLocalRef<jstring> j_str = CREATE_UTF_OR_RETURN(env, "foo");
        result.reset(new ScopedLocalRef<jstring>(std::move(j_str)));
        return 1;
    }(mEnv);

    ScopedUtfChars str(mEnv, result->get());
    EXPECT_EQ(str.c_str(), std::string_view("foo"));
    EXPECT_FALSE(mEnv->ExceptionCheck());
    EXPECT_EQ(ret, 1);
}

class MyString : public std::string {
   public:
    explicit MyString(const char* c_str) : std::string(c_str) {}

    // Force clear the string to catch use-after-free issues.
    ~MyString() { clear(); }
};

// `expr` creates a temporary object and evaluates to it.
TEST_F(LibnativehelperTest, CreateUtfOrReturnExprEvaluatesToTemporary) {
    std::unique_ptr<ScopedLocalRef<jstring>> result;

    jint ret = [&](JNIEnv* env) -> jint {
        ScopedLocalRef<jstring> j_str = CREATE_UTF_OR_RETURN(env, MyString("foo"));
        result.reset(new ScopedLocalRef<jstring>(std::move(j_str)));
        return 1;
    }(mEnv);

    ScopedUtfChars str(mEnv, result->get());
    EXPECT_EQ(str.c_str(), std::string_view("foo"));
    EXPECT_FALSE(mEnv->ExceptionCheck());
    EXPECT_EQ(ret, 1);
}

// `expr` creates a temporary object and evaluates to something else backed by it.
TEST_F(LibnativehelperTest, CreateUtfOrReturnExprEvaluatesToValueBackedByTemporary) {
    std::unique_ptr<ScopedLocalRef<jstring>> result;

    jint ret = [&](JNIEnv* env) -> jint {
        ScopedLocalRef<jstring> j_str = CREATE_UTF_OR_RETURN(env, MyString("foo").c_str());
        result.reset(new ScopedLocalRef<jstring>(std::move(j_str)));
        return 1;
    }(mEnv);

    ScopedUtfChars str(mEnv, result->get());
    EXPECT_EQ(str.c_str(), std::string_view("foo"));
    EXPECT_FALSE(mEnv->ExceptionCheck());
    EXPECT_EQ(ret, 1);
}

TEST_F(LibnativehelperTest, CreateUtfOrReturnVoid) {
    std::unique_ptr<ScopedLocalRef<jstring>> result;

    [&](JNIEnv* env) -> void {
        ScopedLocalRef<jstring> j_str = CREATE_UTF_OR_RETURN_VOID(env, "foo");
        result.reset(new ScopedLocalRef<jstring>(std::move(j_str)));
    }(mEnv);

    ScopedUtfChars str(mEnv, result->get());
    EXPECT_EQ(str.c_str(), std::string_view("foo"));
    EXPECT_FALSE(mEnv->ExceptionCheck());
}

TEST_F(LibnativehelperTest, CreateUtfOrReturnFailed) {
    JNINativeInterface interface;
    interface.NewStringUTF = [](JNIEnv*, const char*) -> jstring { return nullptr; };
    JNIEnv fake_env;
    fake_env.functions = &interface;

    jint ret = [&](JNIEnv* env) -> jint {
        ScopedLocalRef<jstring> j_str = CREATE_UTF_OR_RETURN(env, "foo");
        return 1;
    }(&fake_env);

    EXPECT_EQ(ret, 0);
}

TEST_F(LibnativehelperTest, CreateUtfOrReturnVoidFailed) {
    JNINativeInterface interface;
    interface.NewStringUTF = [](JNIEnv*, const char*) -> jstring { return nullptr; };
    JNIEnv fake_env;
    fake_env.functions = &interface;

    bool execution_completed = false;

    [&](JNIEnv* env) -> void {
        ScopedLocalRef<jstring> j_str = CREATE_UTF_OR_RETURN_VOID(env, "foo");
        execution_completed = true;
    }(&fake_env);

    EXPECT_FALSE(execution_completed);
}

TEST_F(LibnativehelperTest, CreateScopedUtfChars) {
    // Several byte representations of the Java string literal "\u0000\ud834\udd1e".
    constexpr uint16_t kUtf16[] = { 0x0000, 0xd834, 0xdd1e };
    constexpr uint8_t kArtUtf8[] = { 0xc0, 0x80, 0xf0, 0x9d, 0x84, 0x9e };

    // Create a Java string from UTF-16 as it behaves the same regardless of the JVM.
    ScopedLocalRef<jstring> j_str(mEnv, mEnv->NewString((const jchar*) kUtf16, std::size(kUtf16)));

    ScopedUtfChars jni_utf(mEnv, j_str.get());
#if defined(__ANDROID__) || defined(ART_BASE_ADDRESS)
    EXPECT_EQ(jni_utf.size(), sizeof(kArtUtf8));
    EXPECT_EQ(memcmp(jni_utf.c_str(), kArtUtf8, sizeof(kArtUtf8)), 0);
#else
    constexpr uint8_t kModifiedUtf8[] = { 0xc0, 0x80, 0xed, 0xa0, 0xb4, 0xed, 0xb4, 0x9e };

    // On host JVM, GetStringUTFChars returns modified UTF-8 chars.
    EXPECT_EQ(jni_utf.size(), sizeof(kModifiedUtf8));
    EXPECT_EQ(memcmp(jni_utf.c_str(), kModifiedUtf8, sizeof(kModifiedUtf8)), 0);
#endif

    // ScopedArtUtfChars always returns the same chars as ART regardless of the JVM.
    ScopedArtUtfChars art_utf(mEnv, j_str.get());
    EXPECT_EQ(art_utf.size(), sizeof(kArtUtf8));
    EXPECT_EQ(memcmp(art_utf.c_str(), kArtUtf8, sizeof(kArtUtf8)), 0);
}
