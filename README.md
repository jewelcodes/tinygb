# TinyGB
TinyGB is a tiny and portable Game Boy emulator written entirely in C as a hobbyist one-man project. I only wrote this to deepen my understanding of how direct hardware low-level programming works.

## Roadmap
- [x] (Almost) complete implementation of the Z80 CPU
- [x] Monochrome display for the original Game Boy
- [x] User input
- [x] Memory bank controllers to save games and/or play ROMs larger than 32 KiB
- [x] Super Game Boy color functions
- [ ] Super Game Boy borders
- [ ] Game Boy Color functions
- [ ] Sound output
- [ ] Serial port and linking
- [ ] (Possibly) make the UI more user-friendly?

## Playable Games
The links below point to screenshots of the gameplay.
* [Tetris](https://imgur.com/a/hQExSti) (as of 2 May 2022)
* [Pokemon Red](https://imgur.com/a/9qihzw9) (as of 5 May 2022)
* [Pokemon Yellow](https://imgur.com/a/OYxf7MG) (as of 9 May 2022)

## Requirements
```sh
sudo apt install libsdl2 libsdl2-dev
```

## Building
```sh
git clone https://github.com/jewelcodes/tinygb.git
cd tinygb
make
```
