# About

Attempts to translate x86 assembly into mos6502 assembly.

# Why?

Why not? I figured I just barely knew some x86 and some 6502, so why not learn some more about both. Besides, this project can actually have some interesting applications.

# Example

After compilation (requires a full C++14 compiler), you can run something like this:

```assembly_x86
// test.asm
main:
	movb	$1, 53280
	xorl	%eax, %eax
	ret
```

```bash
cat test.asm | x86-to-6502
```

And get this output:

```
main:
	ldy #$1
	sty 53280
	lda $00
	rts 
```

# Caveats

 * Nothing is guaranteed. This could break your computer. Who knows?
 * All values are truncated to 8 bit. We have no support for 16bit math or pointers yet
 * Only as many instructions are supported as I have needed to support to get my test cases working

# To Do

*Everything*

Well, lots of things.

## Better code organization

I really had no idea what I was doing when I started this. So, like all code, it's going to need some improvements along the way to make it more organized and more maintainable.

## Improvements

 * Keep track of input source lines and add comments in output to show where lines came from, for learning / debugging purposes
 * Better / more efficient translation
 * Support for 16bit math? Maybe? We need to at least be able to do 16bit pointer math
 * Consider supporting the "Sweet 16" virtual 16bit CPU that Woz designed?
 * Maybe for 16bit operation we try to detect if the input code is working on 8bit registers or 16+bit registers and do the right thing?
 * Support for more CPU instructions
 * A test suite


