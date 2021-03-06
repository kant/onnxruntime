;++
;
; Copyright (c) Microsoft Corporation. All rights reserved.
;
; Licensed under the MIT License.
;
; Module Name:
;
;   LogisticKernelFma3.asm
;
; Abstract:
;
;   This module implements a kernel for computing the logistic function for a
;   buffer of elements.
;
;   This implementation uses AVX fused multiply/add instructions.
;
;--

        .xlist
INCLUDE mlasi.inc
INCLUDE SgemmKernelCommon.inc
        .list

        EXTERN  MlasMaskMoveAvx:NEAR
        EXTERN  MlasLogisticConstants:NEAR

;
; Structure layout for the logistic constants block.
;

LogisticConstants STRUCT

        LowerRange DWORD ?
        UpperRange DWORD ?
        alpha_9 DWORD ?
        alpha_7 DWORD ?
        alpha_5 DWORD ?
        alpha_3 DWORD ?
        alpha_1 DWORD ?
        beta_10 DWORD ?
        beta_8 DWORD ?
        beta_6 DWORD ?
        beta_4 DWORD ?
        beta_2 DWORD ?
        beta_0 DWORD ?
        one_half DWORD ?

LogisticConstants ENDS

;
; Stack frame layout for the logistic kernel.
;

LogisticKernelFrame STRUCT

        SavedXmm6 OWORD ?
        SavedXmm7 OWORD ?
        SavedXmm8 OWORD ?
        SavedXmm9 OWORD ?
        SavedXmm10 OWORD ?
        SavedXmm11 OWORD ?
        SavedXmm12 OWORD ?
        SavedXmm13 OWORD ?
        SavedXmm14 OWORD ?
        SavedXmm15 OWORD ?
        Padding0 QWORD ?
        Padding1 QWORD ?
        CountN QWORD ?
        ReturnAddress QWORD ?
        PreviousP1Home QWORD ?
        PreviousP2Home QWORD ?
        PreviousP3Home QWORD ?
        PreviousP4Home QWORD ?

LogisticKernelFrame ENDS

;++
;
; Routine Description:
;
;   This routine implements the a vectorized kernel for the logistic function.
;
; Arguments:
;
;   Input (rcx) - Supplies the input buffer.
;
;   Output (rdx) - Supplies the output buffer.
;
;   N (r8)  - Supplies the number of elements to process.
;
; Return Value:
;
;   None.
;
;--

        NESTED_ENTRY MlasLogisticKernelFma3, _TEXT

        alloc_stack (LogisticKernelFrame.ReturnAddress)

        save_xmm128_avx xmm6,LogisticKernelFrame.SavedXmm6
        save_xmm128_avx xmm7,LogisticKernelFrame.SavedXmm7
        save_xmm128_avx xmm8,LogisticKernelFrame.SavedXmm8
        save_xmm128_avx xmm9,LogisticKernelFrame.SavedXmm9
        save_xmm128_avx xmm10,LogisticKernelFrame.SavedXmm10
        save_xmm128_avx xmm11,LogisticKernelFrame.SavedXmm11
        save_xmm128_avx xmm12,LogisticKernelFrame.SavedXmm12
        save_xmm128_avx xmm13,LogisticKernelFrame.SavedXmm13
        save_xmm128_avx xmm14,LogisticKernelFrame.SavedXmm14
        save_xmm128_avx xmm15,LogisticKernelFrame.SavedXmm15

        END_PROLOGUE

        lea     rax,MlasLogisticConstants
        vbroadcastss ymm4,LogisticConstants.LowerRange[rax]
        vbroadcastss ymm5,LogisticConstants.UpperRange[rax]
        vbroadcastss ymm6,LogisticConstants.alpha_9[rax]
        vbroadcastss ymm7,LogisticConstants.alpha_7[rax]
        vbroadcastss ymm8,LogisticConstants.alpha_5[rax]
        vbroadcastss ymm9,LogisticConstants.alpha_3[rax]
        vbroadcastss ymm10,LogisticConstants.alpha_1[rax]
        vbroadcastss ymm11,LogisticConstants.beta_10[rax]
        vbroadcastss ymm12,LogisticConstants.beta_6[rax]
        vbroadcastss ymm13,LogisticConstants.beta_4[rax]
        vbroadcastss ymm14,LogisticConstants.beta_2[rax]
        vbroadcastss ymm15,LogisticConstants.beta_0[rax]

        sub     r8,8
        jb      ProcessRemainingCount

ComputeLogisticBy8Loop:
        vmaxps  ymm0,ymm4,YMMWORD PTR [rcx]     ; clamp lower bound
        vmovaps ymm2,ymm7
        vminps  ymm0,ymm5,ymm0                  ; clamp upper bound
        vmulps  ymm1,ymm0,ymm0                  ; x2
        vbroadcastss ymm3,LogisticConstants.beta_8[rax]
        vfmadd231ps ymm2,ymm1,ymm6              ; p = x2 * alpha_9 + alpha_7
        vfmadd213ps ymm2,ymm1,ymm8              ; p = x2 * p + alpha_5
        vfmadd213ps ymm2,ymm1,ymm9              ; p = x2 * p + alpha_3
        vfmadd213ps ymm2,ymm1,ymm10             ; p = x2 * p + alpha_1
        vfmadd231ps ymm3,ymm1,ymm11             ; q = x2 * beta_10 + beta_8
        vfmadd213ps ymm3,ymm1,ymm12             ; q = x2 * q + beta_6
        vfmadd213ps ymm3,ymm1,ymm13             ; q = x2 * q + beta_4
        vfmadd213ps ymm3,ymm1,ymm14             ; q = x2 * q + beta_2
        vfmadd213ps ymm3,ymm1,ymm15             ; q = x2 * q + beta_0
        vmulps  ymm2,ymm0,ymm2                  ; p = x * p
        vbroadcastss ymm0,LogisticConstants.one_half[rax]
        vdivps  ymm2,ymm2,ymm3
        vxorps  ymm3,ymm3,ymm3
        vaddps  ymm0,ymm2,ymm0                  ; logistic = p / q + 0.5
        vmaxps  ymm0,ymm3,ymm0                  ; clamp lower bound
        add     rcx,8*4                         ; advance input by 8 elements
        vmovups YMMWORD PTR [rdx],ymm0
        add     rdx,8*4                         ; advance output by 8 elements
        sub     r8,8
        jae     ComputeLogisticBy8Loop

ProcessRemainingCount:
        add     r8,8                            ; correct for over-subtract above
        jz      ExitKernel
        mov     DWORD PTR LogisticKernelFrame.CountN[rsp],r8d
        vbroadcastss ymm2,DWORD PTR LogisticKernelFrame.CountN[rsp]
        vpcmpgtd ymm2,ymm2,YMMWORD PTR [MlasMaskMoveAvx]
        vmaskmovps ymm0,ymm2,YMMWORD PTR [rcx]
        vmaxps  ymm0,ymm4,ymm0                  ; clamp lower bound
        vminps  ymm0,ymm5,ymm0                  ; clamp upper bound
        vmulps  ymm1,ymm0,ymm0                  ; x2
        vbroadcastss ymm3,LogisticConstants.beta_8[rax]
        vfmadd231ps ymm7,ymm1,ymm6              ; p = x2 * alpha_9 + alpha_7
        vfmadd213ps ymm7,ymm1,ymm8              ; p = x2 * p + alpha_5
        vfmadd213ps ymm7,ymm1,ymm9              ; p = x2 * p + alpha_3
        vfmadd213ps ymm7,ymm1,ymm10             ; p = x2 * p + alpha_1
        vfmadd231ps ymm3,ymm1,ymm11             ; q = x2 * beta_10 + beta_8
        vfmadd213ps ymm3,ymm1,ymm12             ; q = x2 * q + beta_6
        vfmadd213ps ymm3,ymm1,ymm13             ; q = x2 * q + beta_4
        vfmadd213ps ymm3,ymm1,ymm14             ; q = x2 * q + beta_2
        vfmadd213ps ymm3,ymm1,ymm15             ; q = x2 * q + beta_0
        vmulps  ymm7,ymm0,ymm7                  ; p = x * p
        vbroadcastss ymm0,LogisticConstants.one_half[rax]
        vdivps  ymm7,ymm7,ymm3
        vxorps  ymm3,ymm3,ymm3
        vaddps  ymm0,ymm7,ymm0                  ; logistic = p / q + 0.5
        vmaxps  ymm0,ymm3,ymm0                  ; clamp lower bound
        vmaskmovps YMMWORD PTR [rdx],ymm2,ymm0

ExitKernel:
        vzeroupper
        vmovaps xmm6,LogisticKernelFrame.SavedXmm6[rsp]
        vmovaps xmm7,LogisticKernelFrame.SavedXmm7[rsp]
        vmovaps xmm8,LogisticKernelFrame.SavedXmm8[rsp]
        vmovaps xmm9,LogisticKernelFrame.SavedXmm9[rsp]
        vmovaps xmm10,LogisticKernelFrame.SavedXmm10[rsp]
        vmovaps xmm11,LogisticKernelFrame.SavedXmm11[rsp]
        vmovaps xmm12,LogisticKernelFrame.SavedXmm12[rsp]
        vmovaps xmm13,LogisticKernelFrame.SavedXmm13[rsp]
        vmovaps xmm14,LogisticKernelFrame.SavedXmm14[rsp]
        vmovaps xmm15,LogisticKernelFrame.SavedXmm15[rsp]
        add     rsp,(LogisticKernelFrame.ReturnAddress)

        BEGIN_EPILOGUE

        ret

        NESTED_END MlasLogisticKernelFma3, _TEXT

        END
