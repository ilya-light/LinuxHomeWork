savedcmd_/home/ilya/repos/LinuxHomeWork/kernel/myfs.o := x86_64-linux-gnu-ld -m elf_x86_64 -z noexecstack --no-warn-rwx-segments   -r -o /home/ilya/repos/LinuxHomeWork/kernel/myfs.o @/home/ilya/repos/LinuxHomeWork/kernel/myfs.mod  ; ./tools/objtool/objtool --hacks=jump_label --hacks=noinstr --hacks=skylake --ibt --orc --retpoline --rethunk --sls --static-call --uaccess --prefix=16  --link  --module /home/ilya/repos/LinuxHomeWork/kernel/myfs.o

/home/ilya/repos/LinuxHomeWork/kernel/myfs.o: $(wildcard ./tools/objtool/objtool)
