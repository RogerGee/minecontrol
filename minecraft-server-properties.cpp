// minecraft-server-properties.cpp
#include "minecraft-server-properties.h"
using namespace rtypes;
using namespace minecraft_controller;

void mcraft_level_type::_putValue(rstream& stream) const
{
    switch ( get_value() )
    {
    case mcraft_level_flat:
        stream << "FLAT";
        break;
    case mcraft_level_largebiomes:
        stream << "LARGEBIOMES";
        break;
    case mcraft_level_amplified:
        stream << "AMPLIFIED";
        break;
    default:
        stream << "DEFAULT";
        break;
    }
}
