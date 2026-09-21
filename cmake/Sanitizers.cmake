# Sanitizers.cmake

function(target_enable_sanitizers TARGET_NAME)
    if(QRoute_SANITIZE)
        if(MSVC)
            target_compile_options(${TARGET_NAME} PRIVATE /fsanitize=address)
        else()
            target_compile_options(${TARGET_NAME} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
            target_link_options(${TARGET_NAME} PRIVATE
                -fsanitize=address,undefined
            )
        endif()
    endif()
endfunction()