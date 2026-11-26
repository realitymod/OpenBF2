// Player input action storage.
#include "PlayerAction.h"
#include "../../IO/Input/PlayerInput.h"
#include "../../Core/Debug.h"

namespace dice {
namespace hfe {

namespace {

// Button-input mapping, terminated by 64. Analogue inputs 0..7 are handled
// separately by setAxis; preserve this table's order and values.
const int playerInputIsMappedArray[] = {
    32, 34, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30,  8, 31, 47,  9, 10, 13, 12, 37, 38, 62, 36, 35, 39,
    40, 41, 42, 11, 43, 44, 33, 45, 46,
    64      // terminator
};

// [CHOSEN, not recovered] Project Reality's client and server quantize axes to 1/2048 instead of
// retail's hundredths (PE client 0x5BC5F0, server 0x55BDF0, and the set/get axis loops). A PR
// client's actions decode correctly only with the same scale.
const float kAxisScale = 2048.0f;

// A button is pressed only when input strictly exceeds 0.5.
const float kPressedThreshold = 0.5f;   // ds:0x888B170, compared with fucomip - strictly greater

} // anonymous namespace

// Quantizes an axis to 1/kAxisScale by truncating through a signed short. Clamps the
// scaled value symmetrically to ±32767, excluding the most-negative short value.
float convertAxisToInternalResolution(float value)
{
    float scaled = value * kAxisScale;

    if (scaled > 32767.0f)                          // ds:0x888B3A0
        scaled = 32767.0f;
    else if (!(scaled >= -32767.0f))                // ds:0x888B3A4, via fcmovbe
        scaled = -32767.0f;

    return (float)(short)(int)scaled / kAxisScale;
}

// Stores the mapped input as a quantized short; unmapped input stores zero. Requires
// an axis within this action's six entries. Preserve the float quantization round trip.
void PlayerAction::setAxis(const io::PlayerInput& input, int axis)
{
    float value = 0.0f;

    if (axis <= 63)
    {
        // The same 64-bit mapping mask set() consults, split into the ELF's two halves.
        const unsigned int mapLow  = (unsigned int)(input.mappingFlags & 0xFFFFFFFFULL);
        const unsigned int mapHigh = (unsigned int)(input.mappingFlags >> 32);

        const bool mapped = ((axis & 0x20) ? (mapHigh >> (axis & 0x1F))
                                           : (mapLow  >> (axis & 0x1F))) & 1u;
        if (mapped)
            value = input.inputs[axis];
    }

    m_axis[axis] = (short)(int)(convertAxisToInternalResolution(value) * kAxisScale);
}

// Copies action fields in their defined order while leaving tail padding untouched.
void PlayerAction::set(const PlayerAction& other)
{
    for (int i = 0; i < 6; ++i)          // +0x04..+0x0F, six words, unrolled in the PE
        m_axis[i] = other.m_axis[i];
    m_buttonMaskLow  = other.m_buttonMaskLow;    // +0x10
    m_buttonMaskHigh = other.m_buttonMaskHigh;   // +0x14
    m_inputIndex = other.m_inputIndex;   // +0x00, copied last
    m_flag       = other.m_flag;         // +0x18; m_pad19 is NOT copied
}
void PlayerAction::set(const io::PlayerInput& input)
{
    for (int axis = 0; axis <= 5; ++axis)
        setAxis(input, axis);

    m_buttonMaskLow  = 0;
    m_buttonMaskHigh = 0;
    m_flag           = 0;

    // The 64-bit mapping mask, split into the two halves the ELF works on.
    const unsigned int mapLow  = (unsigned int)(input.mappingFlags & 0xFFFFFFFFULL);
    const unsigned int mapHigh = (unsigned int)(input.mappingFlags >> 32);

    for (const int* p = playerInputIsMappedArray; *p != 64; ++p)
    {
        const int idx = *p;
        if (idx > 63)                       // dead - the array tops out at 62
            continue;

        const bool mapped = ((idx & 0x20) ? (mapHigh >> (idx & 0x1F))
                                          : (mapLow  >> (idx & 0x1F))) & 1u;
        if (!mapped)
            continue;

        if (!(input.inputs[idx] > kPressedThreshold))    // strictly greater
            continue;

        const int bit = idx - 8;
        if (bit > 63)                       // dead - see the header block. Reproduced.
        {
            Debug(DEBUGTYPE_6, "Game/Common/PlayerAction.cpp", 186, __FILE__)
                << "playeraction, input out of range, only supports 32:buttons at the momemnt. bit:"
                << bit;
        }

        if (bit > 31)
            m_buttonMaskHigh |= 1u << (idx - 40);
        else
            m_buttonMaskLow  |= 1u << bit;
    }

    m_flag = input.flag;
}


// Input mapping helpers retained locally until the io::Input template owns them.
// They preserve mapping checks and range validation used while decoding player actions.
namespace {

// Accesses the low or high 32-bit half of the 64-bit mapping mask.
unsigned int mapHalf(const io::PlayerInput& input, int half)
{
    return (unsigned int)(input.mappingFlags >> (half * 32));
}

void setMapHalf(io::PlayerInput& input, int half, unsigned int v)
{
    const unsigned long long keep = half ? (input.mappingFlags & 0xFFFFFFFFULL)
                                         : (input.mappingFlags & 0xFFFFFFFF00000000ULL);
    input.mappingFlags = keep | ((unsigned long long)v << (half * 32));
}

bool inputIsMapped(const io::PlayerInput& input, int idx)
{
    return ((mapHalf(input, (idx & 0x20) ? 1 : 0) >> (idx & 0x1F)) & 1u) != 0;
}

// . NO range check - unlike setInput, this one shifts by whatever it is given.
void setInputIsMapped(io::PlayerInput& input, int idx, bool mapped)
{
    const int          half = (idx & 0x20) ? 1 : 0;
    const unsigned int bit  = 1u << (idx & 0x1F);
    const unsigned int cur  = mapHalf(input, half);
    setMapHalf(input, half, mapped ? (cur | bit) : (cur & ~bit));   // both arms read: OR / AND-NOT
}

// Unmapped inputs retain their previous values.
void setInput(io::PlayerInput& input, int idx, float value)
{
    if ((unsigned int)idx > 63u)
    {
        // dead from this file - every index reaching here is an axis 0..5 or an entry of
        // playerInputIsMappedArray, which tops out at 62. Reproduced anyway, same reasoning as
        // set()'s dead Debug. THE leading "2" IS dice'S, verbatim from ds:0x888B3DE.
        Debug(DEBUGTYPE_6, "IO/Input/Input.h", 203, __FILE__)
            << "2 setInput out of range: " << idx;
        return;
    }
    if (inputIsMapped(input, idx))
        input.inputs[idx] = value;
}

void setInput(io::PlayerInput& input, int idx, bool pressed)
{
    // the ELF's bool overload converts with cvtsi2ss, so exactly 0.0f / 1.0f
    setInput(input, idx, pressed ? 1.0f : 0.0f);
}

} // anonymous namespace

// Decodes one stored axis in 1/kAxisScale units and marks the destination input mapped before
// writing it. Requires axis 0–5; the source array access is unchecked.
void PlayerAction::getAxis(io::PlayerInput& out, int axis) const
{
    setInputIsMapped(out, axis, true);
    setInput(out, axis, (float)m_axis[axis] / kAxisScale);
}

// Expands an action into player input, zeroing input values before decoding axes and buttons.
// Marks each decoded input as mapped before setting it; mapping is required for the write to apply.
bool PlayerAction::get(io::PlayerInput& out) const
{
    for (int i = 0; i <= 63; ++i)
        out.inputs[i] = 0.0f;

    for (int axis = 0; axis <= 5; ++axis)
        getAxis(out, axis);

    for (const int* p = playerInputIsMappedArray; *p != 64; ++p)
    {
        const int idx = *p;
        const int bit = idx - 8;

        setInputIsMapped(out, idx, true);       // first, or the store below is dropped

        if (bit > 31)
            setInput(out, idx, (m_buttonMaskHigh & (1u << (idx - 40))) != 0);
        else
            setInput(out, idx, (m_buttonMaskLow  & (1u << bit)) != 0);
    }

    out.flag = m_flag;
    return true;                                // the ELF's `mov eax, 1` - see the header
}

} // namespace hfe
} // namespace dice
