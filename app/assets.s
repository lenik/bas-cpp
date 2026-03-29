.section .assets,"a"
.align 4                 /* Ensures 4-byte alignment */
.global bas_cpp_assets_data_start
bas_cpp_assets_data_start:
    .incbin "assets.zip"
.global bas_cpp_assets_data_end
bas_cpp_assets_data_end:
