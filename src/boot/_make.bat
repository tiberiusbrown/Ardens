python bin2c.py -i arduboy3k-bootloader-game-sda.hex -a ARDENS_BOOT_GAME -o boot_game.c
python bin2c.py -i arduboy3k-bootloader-menu-sda.hex -a ARDENS_BOOT_MENU -o boot_menu.c
python bin2c.py -i flashcart_empty.bin -a ARDENS_BOOT_FLASHCART -o boot_flashcart.c
