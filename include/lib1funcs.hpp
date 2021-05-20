
static constexpr std::string_view __mulhi3 = 
R"(
;;; based on protocol from gcc's calling conventions for AVR

;;; R25:R24 = R23:R22 * R25:R24
;;; Clobbers: __tmp_reg__, R21..R23

__mulhi3:
        mov __temp_reg__,r24
        mov r21,r25
        ldi r25,0
        ldi r24,0
        cp __temp_reg__,__zero_reg__
        cpc r21,__zero_reg__
        breq .__mulhi3_L5
.__mulhi3_L4:
        sbrs __temp_reg__,0
        rjmp .__mulhi3_L3
        add r24,r22
        adc r25,r23
.__mulhi3_L3:
        lsr r21
        ror __temp_reg__
        lsl r22
        rol r23
        cp __temp_reg__,__zero_reg__
        cpc r21,__zero_reg__
        brne .__mulhi3_L4
        ret
.__mulhi3_L5:
        ret
)";


