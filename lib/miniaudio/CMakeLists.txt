add_library(miniaudio STATIC)

target_include_directories(miniaudio 
    PUBLIC
        include
    PRIVATE
        include/miniaudio
        
)

target_sources(miniaudio PRIVATE
    miniaudio.c
    
)

if (NOT MSVC)
    if(ANDROID)
        target_link_libraries(miniaudio PUBLIC m dl)
    else()
        target_link_libraries(miniaudio PUBLIC m dl pthread)
    endif()
endif()