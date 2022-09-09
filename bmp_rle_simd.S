.intel_syntax noprefix
.global bmp_rle_simd

.text
/*
	@Signature
	extern unsigned int bmp_rle_simd(const char *source, RLEPIXEL *destination, const int width, const int height)

	@Parameters
	source -> rdi: array address of pixels to compress
	destination -> rsi: array address of compressed pixels
	width -> edx: width of image
	height -> ecx: height of image

	@Returns
	Count of compressed pixels

	r11d = padding
	r11b = encoded
	rax = i
	r8 = j, but zero has here the value of the startaddress of the destination in rsi in order to increase efficiency by avoiding calculating additions every time when accessing the destination array.
	and r8 has here a value multiplied with 2, because entries of the RLEPIXEL array have the size of 2 bytes.
	r9d = n
	r10 = checkpoint
	ebx = m
	r12b = current
	r13b = count, copy is r13d
	r14d = limit, count on else area

	PS:
	Note for modifying the destination array: often used 2 times access to this memory area, first for the .count value of the struct,
	then by adding one byte (+1) for the .color value. accessing [r8] and [r8 + 1] in the most common cases.
*/
bmp_rle_simd:
	// save callee-saved registers
	sub rsp, 48
	mov qword ptr [rsp], rbx
	mov qword ptr [rsp + 8], r12
	mov qword ptr [rsp + 16], r13
	mov qword ptr [rsp + 24], r14
	mov qword ptr [rsp + 32], r15

	and qword ptr [rsp + 40], 0

	mov eax, edx
	and eax, 0x3
	jz .LwidthMod4Zero
	mov r11d, 4
	sub r11d, eax
	jmp .LpaddingEnd

.LwidthMod4Zero:
	xor r11d, r11d

.LpaddingEnd:
	mov dword ptr [rsp + 40], r11d
	/*
		i - current index in source
		int i = 0
		int checkpoint = 0
		j - current index in destination
		int j = 0
		switch between encoded and absolute mode
		uint8_t encoded = 0
		loop over lines
	*/
	mov rax, rdi
	mov r10, rdi
	mov r8, rsi
	xor r11b, r11b
	xor ebx, ebx

.LloopOverLines:
	// for (int m = 0; m < height; m++)
	cmp ebx, ecx
	jge .LloopOverLinesEnd
	xor r9d, r9d // n to loop over pixels in the same line

.LloopOverPixels:
	// while (n < width)
	cmp r9d, edx
	jnb .LloopOverPixelsEnd // only way to leave this while loop, code under that will always jump at some point to .LloopOverPixels.
	// if (encoded)
	test r11b, r11b
	jz .LnotEncoded // jump to else, one code line before else, there will be a jmp .LloopOverPixels instruction so that in this case the else code will not be executed.
	.Lencoded:
	mov r12b, [rax]
	inc rax
	inc r9d
	mov r13b, 1
	// while (source[i] == current && n < width && count < 255)

.LwhileSamePixel:
	cmp r12b, [rax]
	jne .LwhileSamePixelEnd // source[i] == current
	cmp r9d, edx
	jge .LwhileSamePixelEnd // n < width
	cmp r13b, 0xff
	jz .LwhileSamePixelEnd // count < 255
	inc r13b
	inc rax
	inc r9d
	jmp .LwhileSamePixel

.LwhileSamePixelEnd:
	// write compressed pixel(s) to destination array
	mov [r8], r13b
	mov [r8 + 1], r12b
	add r8, 2
	// switch back to absolute mode and adjust the checkpoint
	xor r11b, r11b
	mov r10, rax
	// end if encoded
	jmp .LloopOverPixels

.LnotEncoded:
	mov r13d, r9d
	mov r12b, [rax]
	mov r14d, 1
	inc rax
	inc r9d
	// If we're at the beginning of a run, 3 consecutives bytes should be compressed with encoded mode
	// if (n < width && source[i] == current && n + 1 < width && source[i + 1] == current)
	cmp r9d, edx
	jae .LwhileCheck
	cmp [rax], r12b
	jne .LwhileCheck
	lea r15, [r9d + 1]
	cmp r15d, edx
	jae .LwhileCheck
	cmp [rax + 1], r12b
	jne .LwhileCheck
	mov rax, r10
	mov r9d, r13d
	inc r11b
	jmp .Lencoded
	// calculate count of equal consecutive bits on the same line
	// while (((source[i] != current && n < width) || (source[i + 1] != current && n + 1 < width) || (source[i + 2] != current && n + 2 < width)) && limit < 255)

.LwhileCheck:
	cmp [rax], r12b
	je .LwhileFirstOther
	cmp r9d, edx
	jnb .LwhileFirstOther
	jmp .LwhileCheckTrue

.LwhileFirstOther:
	cmp [rax + 1], r12b
	je .LwhileSecondOther
	mov r15d, r9d
	inc r15d
	cmp r15d, edx
	jnb .LwhileSecondOther
	jmp .LwhileCheckTrue

.LwhileSecondOther:
	cmp [rax + 2], r12b
	je .LwhileCheckFalse
	mov r15d, r9d
	add r15d, 2
	cmp r15d, edx
	jnb .LwhileCheckFalse

.LwhileCheckTrue:
	cmp r14d, 255
	je .LwhileCheckFalse
	inc r14d
	mov r12b, [rax]
	inc rax
	inc r9d
	jmp .LwhileCheck

.LwhileCheckFalse:
	// if we're still on the same line, go back one step and switch to encoded mode
	// if (n < width)
	cmp r9d, edx
	jge .LnNotSmallerWidth
	dec rax
	dec r9d
	mov r11b, 1

.LnNotSmallerWidth:
	mov r14, rax
	sub r14, r10
	cmp r14, 3
	jge .LcountNotSmaller3
	mov rax, r10
	mov r9d, r13d
	mov r11b, 1
	jmp .LloopOverPixels

.LcountNotSmaller3:
	and byte ptr [r8], 0
	mov [r8 + 1], r14b
	add r8, 2
	// move checkpoint to i and copy the bytes on the way
	sub rax, 15
	jc .LFastCopyEnd

.LifCheckpointP15SmallerI:
	cmp r10, rax
	jae .LFastCopyEnd
	movdqu xmm0, [r10]
	movdqu [r8], xmm0
	add r8, 16
	add r10, 16
	jmp .LifCheckpointP15SmallerI

.LFastCopyEnd:
	add rax, 15
	// while (checkpoint < i)
	.LwhileCheckpointSmallerI:
	cmp r10, rax
	jae .LcheckpointGreaterEqualI
	// .count = source[checkpoint]
	mov r15b, [r10]
	mov [r8], r15b
	// .color = checkpoint + 1 < i ? source[checkpoint + 1] : 0
	lea r15, [r10 + 1]
	cmp r15, rax
	jae .LcheckpointPlusOneGreaterEqualI
	mov r15b, [r10 + 1]
	mov [r8 + 1], r15b
	jmp .LcheckpointPlusOneSmallerI

.LcheckpointPlusOneGreaterEqualI:
	and byte ptr [r8 + 1], 0

.LcheckpointPlusOneSmallerI:
	add r8, 2
	add r10, 2
	jmp .LwhileCheckpointSmallerI

.LcheckpointGreaterEqualI:
	mov r10, rax
	jmp .LloopOverPixels

.LloopOverPixelsEnd:
	add rax, qword ptr [rsp + 40] // i += padding
	add r10, qword ptr [rsp + 40] // checkpoint += padding
	inc ebx
	cmp ebx, ecx
	// if we're not in the last line, write end of line denotation
	// if (m < height - 1)
	jae .LmNotSmallerHeightMinusOne
	and word ptr [r8], 0
	add r8, 2

.LmNotSmallerHeightMinusOne:
	jmp .LloopOverLines

.LloopOverLinesEnd:
	and byte ptr [r8], 0
	mov byte ptr [r8 + 1], 1
	sub r8, rsi // getting the real value of j by removing the startaddress of the destination array, but here still multiplied with 2.
	lea rax, [r8 + 2]
	shr rax, 1 // because it has the value * 2 because of the 2 bytes size of the RLEPIXEL array, this has now to be shifted right to get the real value that j would have at the c code.

	// restore callee-saved registers
	mov rbx, qword ptr [rsp]
	mov r12, qword ptr [rsp + 8]
	mov r13, qword ptr [rsp + 16]
	mov r14, qword ptr [rsp + 24]
	mov r15, qword ptr [rsp + 32]
	add rsp, 48
	ret
