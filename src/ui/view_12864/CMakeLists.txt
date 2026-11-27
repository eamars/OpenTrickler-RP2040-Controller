ot_module(SRCS menu_view_12864.c
          INCLUDE_DIRS include
          REQUIRES menu
          PRIV_REQUIRES freertos lvgl rp2_sdk uc1701)

# The menu's fonts, made from the BDF files in fonts/ (ADR-010): one family, Helvetica, in three sizes, as the owner
# chose (skill embedded-ui-design): 8 regular for text, 8 bold for titles, 18 bold for the figure a job screen shows.
# The regular one carries the title marks' icons too (gate 5), and the hidden hello's figure is a font of its tiles
set(MENU_FONTS_BDF "${CMAKE_SOURCE_DIR}/fonts")
add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/menu_fonts.c"
    DEPENDS "${SCRIPTS_DIRECTORY}/bdf2lvgl.py" "${MENU_FONTS_BDF}/helvR08.bdf" "${MENU_FONTS_BDF}/helvB08.bdf"
            "${MENU_FONTS_BDF}/helvB18.bdf" "${MENU_FONTS_BDF}/claude_chan.txt"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/bdf2lvgl.py" --out ${SRC_DIRECTORY}/generated/menu_fonts.c
            ${MENU_FONTS_BDF}/helvR08.bdf:menu_font_regular:icons ${MENU_FONTS_BDF}/helvB08.bdf:menu_font_bold
            ${MENU_FONTS_BDF}/helvB18.bdf:menu_font_figure ${MENU_FONTS_BDF}/claude_chan.txt:menu_font_chan:art
    COMMENT "Generating the menu's fonts"
)
target_sources(ot_view_12864 PRIVATE "${SRC_DIRECTORY}/generated/menu_fonts.c")
