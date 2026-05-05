#pragma once

namespace cmf
{

enum class MdAction : char
{
    Add = 'A',
    Modify = 'M',
    Cancel = 'C',
    Clear = 'R',
    Trade = 'T',
    Fill = 'F',
    None = 'N'
};

enum class MdSide : char
{
    Ask = 'A',
    Bid = 'B',
    None = 'N'
};

} // namespace cmf
