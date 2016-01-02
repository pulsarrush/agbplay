#include <cmath>
#include <cassert>

#include "SoundChannel.h"

using namespace std;
using namespace agbplay;

/*
 * public SoundChannel
 */

SoundChannel::SoundChannel(void *owner, SampleInfo sInfo, ADSR env, Note note, uint8_t leftVol, uint8_t rightVol, int16_t pitch, bool fixed)
{
    this->owner = owner;
    this->note = note;
    this->env = env;
    this->sInfo = sInfo;
    this->interPos = 0.0f;
    SetVol(leftVol, rightVol);
    this->fixed = fixed;
    SetPitch(pitch);
    // if instant attack is ative directly max out the envelope to not cut off initial sound
    this->eState = EnvState::INIT;
    this->pos = 0;
}

SoundChannel::~SoundChannel()
{
}

void *SoundChannel::GetOwner()
{
    return owner;
}

float SoundChannel::GetFreq()
{
    return freq;
}

void SoundChannel::SetVol(uint8_t leftVol, uint8_t rightVol)
{
    if (eState < EnvState::REL) {
        this->leftVol = note.velocity * leftVol / 128;
        this->rightVol = note.velocity * rightVol / 128;
    }
}

ChnVol SoundChannel::GetVol()
{
    float envBase = float(fromEnvLevel);
    float envDelta = float(envLevel) - envBase;
    float finalFromEnv = envBase + envDelta * float(envInterStep);
    float finalToEnv = envBase + envDelta * float(envInterStep + 1);
    return ChnVol(
            float(fromLeftVol) * finalFromEnv / 65536,
            float(fromRightVol) * finalFromEnv / 65536,
            float(leftVol) * finalToEnv / 65536,
            float(rightVol) * finalToEnv / 65536);
}

uint8_t SoundChannel::GetMidiKey()
{
    return note.midiKey;
}

bool SoundChannel::IsFixed()
{
    return fixed;
}

void SoundChannel::Release()
{
    if (eState < EnvState::REL) {
        eState = EnvState::REL;
        envInterStep = 0;
    }
}

void SoundChannel::SetPitch(int16_t pitch)
{
    freq = sInfo.midCfreq * powf(2.0f, float(note.midiKey - 60) / 12.0f + float(pitch) / 768.0f);
}

bool SoundChannel::TickNote()
{
    if (eState < EnvState::REL) {
        if (note.length > 0) {
            note.length--;
            if (note.length == 0) {
                eState = EnvState::REL;
                return false;
            }
            return true;
        } else if (note.length == -1) {
            return true;
        } else assert(false);
    } else {
        return false;
    }
}

EnvState SoundChannel::GetState()
{
    return eState;
}

SampleInfo& SoundChannel::GetInfo()
{
    return sInfo;
}

void SoundChannel::StepEnvelope()
{
    switch (eState) {
        case EnvState::INIT:
            fromLeftVol = leftVol;
            fromRightVol = rightVol;
            if (env.att == 0xFF) {
                fromEnvLevel = 0xFF;
            } else {
                fromEnvLevel = 0x0;
            }
            envLevel = env.att;
            envInterStep = 0;
            eState = EnvState::ATK;
            break;
        case EnvState::ATK:
            if (++envInterStep >= INTERFRAMES) {
                fromEnvLevel = envLevel;
                envInterStep = 0;
                int newLevel = envLevel + env.att;
                if (newLevel >= 0xFF) {
                    eState = EnvState::DEC;
                    envLevel = 0xFF;
                } else {
                    envLevel = uint8_t(newLevel);
                }
            }
            break;
        case EnvState::DEC:
            if (++envInterStep >= INTERFRAMES) {
                fromEnvLevel = envLevel;
                envInterStep = 0;
                int newLevel = envLevel * env.dec / 256;
                if (newLevel <= env.sus) {
                    eState = EnvState::SUS;
                    envLevel = env.sus;
                } else {
                    envLevel = uint8_t(newLevel);
                }
            }
            break;
        case EnvState::SUS:
            if (++envInterStep >= INTERFRAMES) {
                fromEnvLevel = envLevel;
                envInterStep = 0;
            }
            break;
        case EnvState::REL:
            if (++envInterStep >= INTERFRAMES) {
                fromEnvLevel = envLevel;
                envInterStep = 0;
                int newLevel = envLevel * env.rel / 256;
                if (newLevel <= 0) {
                    eState = EnvState::DIE;
                    envLevel = 0;
                }
            }
            break;
        case EnvState::DIE:
            if (++envInterStep >= INTERFRAMES) {
                fromEnvLevel = envLevel;
                eState = EnvState::DEAD;
            }
            break;
        case EnvState::DEAD:
            break;
    }
}

void SoundChannel::UpdateVolFade()
{
    fromLeftVol = leftVol;
    fromRightVol = rightVol;
}
