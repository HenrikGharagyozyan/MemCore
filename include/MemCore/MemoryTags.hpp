#pragma once
#include <string_view>
#include <cstddef>

namespace MemCore 
{

    // Enumeration of all subsystems in our future engine
    enum class MemoryTag : std::size_t 
    {
        Unknown = 0,
        Graphics,
        Physics,
        Audio,
        Entities,
        Count // Used to define the size of the statistics arrays
    };

    inline std::string_view ToString(MemoryTag tag) noexcept 
    {
        switch (tag) 
        {
            case MemoryTag::Graphics: return "Graphics";
            case MemoryTag::Physics:  return "Physics";
            case MemoryTag::Audio:    return "Audio";
            case MemoryTag::Entities: return "Entities";
            default:                  return "Unknown";
        }
    }

    // C++17 inline thread_local variable that stores the active tag for the current thread
    inline thread_local MemoryTag g_current_tag = MemoryTag::Unknown;

    // RAII class for conveniently switching subsystem context
    class TagScope 
    {
    private:
        MemoryTag m_previous_tag;

    public:
        explicit TagScope(MemoryTag tag) noexcept 
            : m_previous_tag(g_current_tag) 
        {
            g_current_tag = tag;
        }
        
        ~TagScope() noexcept 
        {
            g_current_tag = m_previous_tag; // Restore the previous tag when leaving scope
        }

        TagScope(const TagScope&) = delete;
        TagScope& operator=(const TagScope&) = delete;
    };

} // namespace MemCore