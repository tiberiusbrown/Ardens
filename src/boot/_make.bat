python bin2c.py -i arduboy3k-bootloader-game-sda.hex -a ARDENS_BOOT_GAME_D1 -o boot_game_d1.c
python bin2c.py -i arduboy3k-bootloader-menu-sda.hex -a ARDENS_BOOT_MENU_D1 -o boot_menu_d1.c
python bin2c.py -i arduboy3k-bootloader-game-devkit.hex -a ARDENS_BOOT_GAME_D2 -o boot_game_d2.c
python bin2c.py -i arduboy3k-bootloader-menu-devkit.hex -a ARDENS_BOOT_MENU_D2 -o boot_menu_d2.c
python bin2c.py -i arduboymini-bootloader-game.hex -a ARDENS_BOOT_GAME_E2 -o boot_game_e2.c
python bin2c.py -i arduboymini-bootloader-menu.hex -a ARDENS_BOOT_MENU_E2 -o boot_menu_e2.c
python bin2c.py -i flashcart_empty.bin -a ARDENS_BOOT_FLASHCART -o boot_flashcart.c
