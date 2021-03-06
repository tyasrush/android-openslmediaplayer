//
//    Copyright (C) 2016 Haruki Hasegawa
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

//#define LOG_TAG "AudioTrack"

#include "oslmp/impl/AudioTrack.hpp"

#include <cxxporthelper/compiler.hpp>
#include <loghelper/loghelper.h>
#include <jni_utils/jni_utils.hpp>

#include "oslmp/impl/AudioFormat.hpp"

namespace oslmp {
namespace impl {

static jmethodID safeGetMethodId(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    jmethodID methodId = env->GetMethodID(clazz, name, sig);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        methodId = 0;
    }
    return methodId;
}

AudioTrack::AudioTrack()
    : cls_(nullptr), obj_(nullptr), m_play_(0), m_pause_(0), m_stop_(0), m_flush_(0), m_release_(0),
      m_get_state_(0), m_get_play_state_(0), m_get_audio_session_(0), m_write_sa_(0), m_write_fa_(0), m_write_bb_(0),
      audio_format_(AudioFormat::ENCODING_INVALID), channel_count_(AudioFormat::CHANNEL_INVALID),
      buffer_size_in_frames_(0), buffer_size_in_bytes_(0)
{
}

AudioTrack::~AudioTrack()
{
}

bool AudioTrack::create(
    JNIEnv *env, int32_t stream_type, int32_t sample_rate, 
    int32_t num_channels, int32_t format, int32_t buffer_size_in_frames, 
    AudioTrack::track_mode_t mode, int32_t session_id) noexcept
{
    if (obj_) {
        return false;
    }

    // AudioTrack(int streamType, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes, int mode)
    jlocal_ref_wrapper<jclass> cls;
    cls.assign(env, env->FindClass("android/media/AudioTrack"), jref_type::local_reference_explicit_delete);
    jmethodID constructor = env->GetMethodID(cls(), "<init>", "(IIIIII)V");

    size_t bytes_per_sample = AudioFormat::get_sample_size_from_encoding(format);
    jint channelConfig = (num_channels == 2) ? AudioFormat::CHANNEL_OUT_STEREO : AudioFormat::CHANNEL_OUT_MONO;
    jint bufferSizeInBytes = buffer_size_in_frames * num_channels * bytes_per_sample;

    LOGD("create(streamType = %d, sampleRateInHz = %d, channelConfig = %d, audioFormat = %d, bufferSizeInBytes = %d, mode = %d)",
        stream_type, sample_rate, channelConfig, format, bufferSizeInBytes, mode);

    jlocal_ref_wrapper<jobject> obj;

    obj.assign(
        env, 
        env->NewObject(
            cls(), constructor,
            static_cast<jint>(stream_type),
            static_cast<jint>(sample_rate),
            channelConfig, static_cast<int32_t>(format),
            bufferSizeInBytes,
            static_cast<int32_t>(mode)),
        jref_type::local_reference_explicit_delete);

    if (!obj) {
        return false;
    }

    const jmethodID m_play = safeGetMethodId(env, cls(), "play", "()V");
    const jmethodID m_pause = safeGetMethodId(env, cls(), "pause", "()V");
    const jmethodID m_stop = safeGetMethodId(env, cls(), "stop", "()V");
    const jmethodID m_flush = safeGetMethodId(env, cls(), "flush", "()V");
    const jmethodID m_release = safeGetMethodId(env, cls(), "release", "()V");
    const jmethodID m_get_state = safeGetMethodId(env, cls(), "getState", "()I");
    const jmethodID m_get_play_state = safeGetMethodId(env, cls(), "getPlayState", "()I");
    const jmethodID m_get_audio_session = safeGetMethodId(env, cls(), "getAudioSessionId", "()I");
    const jmethodID m_set_aux_effect_send_level = safeGetMethodId(env, cls(), "setAuxEffectSendLevel", "(F)I");
    const jmethodID m_attach_aux_effect = safeGetMethodId(env, cls(), "attachAuxEffect", "(I)I");
    const jmethodID m_write_sa = safeGetMethodId(env, cls(), "write", "([SII)I");
    const jmethodID m_write_fa = safeGetMethodId(env, cls(), "write", "([FIII)I");
    const jmethodID m_write_bb = safeGetMethodId(env, cls(), "write", "(Ljava/nio/ByteBuffer;II)I");

    if (!(m_play && m_pause && m_stop && m_flush && m_release && m_get_state && m_get_play_state &&
        m_get_audio_session && m_set_aux_effect_send_level && m_attach_aux_effect && (
      m_write_bb || (m_write_sa && format == AudioFormat::ENCODING_PCM_16BIT) || (m_write_fa && format == AudioFormat::ENCODING_PCM_FLOAT)))) {
        return false;
    }

    // update fields
    cls_ = reinterpret_cast<jclass>(env->NewGlobalRef(cls.detach()));
    obj_ = env->NewGlobalRef(obj.detach());
    m_play_ = m_play;
    m_pause_ = m_pause;
    m_stop_ = m_stop;
    m_flush_ = m_flush;
    m_release_ = m_release;
    m_get_state_ = m_get_state;
    m_get_play_state_ = m_get_play_state;
    m_get_audio_session_ = m_get_audio_session;
    m_set_aux_effect_send_level_ = m_set_aux_effect_send_level;
    m_attach_aux_effect_ = m_attach_aux_effect;
    m_write_sa_ = m_write_sa;
    m_write_fa_ = m_write_fa;
    m_write_bb_ = m_write_bb;
    audio_format_ = format;
    channel_count_ = num_channels;
    buffer_size_in_frames_ = buffer_size_in_frames;
    buffer_size_in_bytes_ = bufferSizeInBytes;

    return true;
}

void AudioTrack::release(JNIEnv *env) noexcept
{
    if (cls_ && obj_ && m_release_) {
        env->CallNonvirtualVoidMethod(obj_, cls_, m_release_);
    }

    if (obj_) {
        env->DeleteGlobalRef(obj_);
    }
    if (cls_) {
        env->DeleteGlobalRef(cls_);
    }

    m_play_ = 0;
    m_pause_ = 0;
    m_stop_ = 0;
    m_flush_ = 0;
    m_release_ = 0;
    m_get_play_state_ = 0;
    m_get_audio_session_ = 0;
    m_write_sa_ = 0;
    m_write_fa_ = 0;
    m_write_bb_ = 0;
    audio_format_ = AudioFormat::ENCODING_INVALID;
    channel_count_ = AudioFormat::CHANNEL_INVALID;
    obj_ = nullptr;
    cls_ = nullptr;
    buffer_size_in_frames_ = 0;
    buffer_size_in_bytes_ = 0;
}

int32_t AudioTrack::play(JNIEnv *env) noexcept
{
    if (CXXPH_UNLIKELY(!(m_play_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    env->CallNonvirtualVoidMethod(obj_, cls_, m_play_);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return SUCCESS;
}

int32_t AudioTrack::pause(JNIEnv *env) noexcept
{
    if (CXXPH_UNLIKELY(!(m_pause_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    env->CallNonvirtualVoidMethod(obj_, cls_, m_pause_);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return SUCCESS;
}

int32_t AudioTrack::stop(JNIEnv *env) noexcept
{
    if (CXXPH_UNLIKELY(!(m_stop_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    env->CallNonvirtualVoidMethod(obj_, cls_, m_stop_);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return SUCCESS;
}

int32_t AudioTrack::flush(JNIEnv *env) noexcept
{
    if (CXXPH_UNLIKELY(!(m_flush_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    env->CallNonvirtualVoidMethod(obj_, cls_, m_flush_);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return SUCCESS;
}

int32_t AudioTrack::write(JNIEnv *env, jshortArray data, size_t offset, size_t size) noexcept
{
    if (CXXPH_UNLIKELY(!(m_write_sa_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    jint result = env->CallNonvirtualIntMethod(
            obj_, cls_, m_write_sa_,
            data, static_cast<jint>(offset), static_cast<jint>(size));

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::write(JNIEnv *env, jfloatArray data, size_t offset, size_t size, AudioTrack::write_mode_t mode) noexcept
{
    if (CXXPH_UNLIKELY(!(m_write_fa_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    jint result = env->CallNonvirtualIntMethod(
            obj_, cls_, m_write_fa_,
            data, static_cast<jint>(offset), static_cast<jint>(size), static_cast<jint>(mode));

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::write(JNIEnv *env, jobject data, size_t size_in_bytes, AudioTrack::write_mode_t mode) noexcept
{
    if (CXXPH_UNLIKELY(!(m_write_bb_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    jint result = env->CallNonvirtualIntMethod(
            obj_, cls_, m_write_bb_,
            data, static_cast<jint>(size_in_bytes), static_cast<jint>(mode));

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::getState(JNIEnv *env) noexcept
{
    if (CXXPH_UNLIKELY(!(m_get_state_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    jint result = env->CallNonvirtualIntMethod(obj_, cls_, m_get_state_);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::getPlayState(JNIEnv *env) noexcept
{
    if (CXXPH_UNLIKELY(!(m_get_play_state_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    jint result = env->CallNonvirtualIntMethod(obj_, cls_, m_get_play_state_);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::getAudioSessionId(JNIEnv *env) noexcept
{
    if (CXXPH_UNLIKELY(!(m_get_audio_session_))) {
        return 0;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return 0;
    }

    jint result = env->CallNonvirtualIntMethod(obj_, cls_, m_get_audio_session_);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return 0;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::setAuxEffectSendLevel(JNIEnv *env, float level) noexcept {
    if (CXXPH_UNLIKELY(!(m_set_aux_effect_send_level_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    jint result = env->CallNonvirtualIntMethod(obj_, cls_, m_set_aux_effect_send_level_, level);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::attachAuxEffect(JNIEnv *env, int effect_id) noexcept {
    if (CXXPH_UNLIKELY(!(m_attach_aux_effect_))) {
        return ERROR_INVALID_OPERATION;
    }

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        return ERROR_INVALID_OPERATION;
    }

    jint result = env->CallNonvirtualIntMethod(obj_, cls_, m_attach_aux_effect_, effect_id);

    if (CXXPH_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionClear();
        return ERROR_INVALID_OPERATION;
    }

    return static_cast<int32_t>(result);
}

int32_t AudioTrack::getAudioFormat() const noexcept
{
    return audio_format_;
}

int32_t AudioTrack::getBufferSizeInFrames() const noexcept
{
    return buffer_size_in_frames_;
}

int32_t AudioTrack::getBufferSizeInBytes() const noexcept
{
    return buffer_size_in_bytes_;
}

int32_t AudioTrack::getChannelCount() const noexcept
{
    return channel_count_;
}

bool AudioTrack::supportsByteBufferMethods() const noexcept
{
    return (m_write_bb_ != 0);
}

}
}
