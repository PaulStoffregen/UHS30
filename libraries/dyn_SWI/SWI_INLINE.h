/*
 * File:   SWI_INLINE.h
 * Author: xxxajk@gmail.com
 *
 * Created on December 5, 2014, 9:40 AM
 *
 * This is the actual library.
 * There are no 'c' or 'cpp' files.
 *
 */
#ifdef DYN_SWI_H
#ifndef SWI_INLINE_H
#define	SWI_INLINE_H

#ifndef SWI_MAXIMUM_ALLOWED
#define SWI_MAXIMUM_ALLOWED 4
#endif



#if defined(__arm__) || defined(ARDUINO_ARCH_PIC32)
static char dyn_SWI_initied = 0;
static dyn_SWI* dyn_SWI_LIST[SWI_MAXIMUM_ALLOWED];
static dyn_SWI* dyn_SWI_EXEC[SWI_MAXIMUM_ALLOWED];
#if defined(__arm__)
#if defined(__USE_CMSIS_VECTORS__)
extern "C" {
        void (*_VectorsRam[VECTORTABLE_SIZE])(void)__attribute__((aligned(VECTORTABLE_ALIGNMENT)));
}
#else

__attribute__((always_inline)) static inline void __DSB(void) {
        __asm__ volatile ("dsb");
}
#endif // defined(__USE_CMSIS_VECTORS__)
#else // defined(__arm__)
__attribute__((always_inline)) static inline void __DSB(void) {
        __asm__ volatile ("sync" : : : "memory");
}
#endif // defined(__arm__)

/**
 * Execute queued class ISR routines.
 */
#if defined(ARDUINO_ARCH_PIC32)
static p32_regset *ifs = ((p32_regset *) & IFS0) + (SWI_IRQ_NUM / 32); //interrupt flag register set
static p32_regset *iec = ((p32_regset *) & IEC0) + (SWI_IRQ_NUM / 32); //interrupt enable control reg set
static uint32_t swibit = 1 << (SWI_IRQ_NUM % 32);

void
#if defined(__PIC32MZXX__)
        __attribute__((nomips16,at_vector(SWI_VECTOR),interrupt(SWI_IPL)))
#else
        __attribute__((interrupt(),nomips16))
#endif
        softISR(void) {
#else
#if defined(ARDUINO_spresense_ast)
unsigned int softISR(void) {
#else
void softISR(void) {
#endif
#endif

        //
        // TO-DO: Perhaps limit to 8, and inline this?
        //


        // Make a working copy, while clearing the queue.
        noInterrupts();
#if defined(ARDUINO_ARCH_PIC32)
        //ifs->clr = swibit;
#endif
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) {
                dyn_SWI_EXEC[i] = dyn_SWI_LIST[i];
                dyn_SWI_LIST[i] = NULL;
        }
        __DSB();
        interrupts();

        // Execute each class SWI
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) {
                if(dyn_SWI_EXEC[i]) {
#if defined(__DYN_SWI_DEBUG_LED__)
                        digitalWrite(__DYN_SWI_DEBUG_LED__, HIGH);
#endif
                        dyn_SWI_EXEC[i]->dyn_SWISR();
#if defined(__DYN_SWI_DEBUG_LED__)
                        digitalWrite(__DYN_SWI_DEBUG_LED__, LOW);
#endif
                }
        }
#if defined(ARDUINO_ARCH_PIC32)
        noInterrupts();
        if(!dyn_SWI_EXEC[0]) ifs->clr = swibit;
        interrupts();
#endif
#if defined(ARDUINO_spresense_ast)
        return 0;
#endif
}

#define DDSB() __DSB()
#endif


#if defined(__arm__)
#ifndef interruptsStatus
#define interruptsStatus() __interruptsStatus()
static inline unsigned char __interruptsStatus(void) __attribute__((always_inline, unused));

static inline unsigned char __interruptsStatus(void) {
        unsigned int primask;
        asm volatile ("mrs %0, primask" : "=r" (primask));
        if(primask) return 0;
        return 1;
}
#endif

/**
 * Initialize the Dynamic (class) Software Interrupt
 */
static void Init_dyn_SWI(void) {
        if(!dyn_SWI_initied) {
#if defined(__USE_CMSIS_VECTORS__)
                uint32_t *X_Vectors = (uint32_t*)SCB->VTOR;
                for(int i = 0; i < VECTORTABLE_SIZE; i++) {
                        _VectorsRam[i] = reinterpret_cast<void (*)()>(X_Vectors[i]); /* copy vector table to RAM */
                }
                /* relocate vector table */
                noInterrupts();
                SCB->VTOR = reinterpret_cast<uint32_t>(&_VectorsRam);
                DDSB();
                interrupts();
#endif
#if !defined(ARDUINO_spresense_ast)
                for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) dyn_SWI_LIST[i] = NULL;
                noInterrupts();
                _VectorsRam[SWI_IRQ_NUM + 16] = reinterpret_cast<void (*)()>(softISR);
                DDSB();
                interrupts();
                NVIC_SET_PRIORITY(SWI_IRQ_NUM, 255);
                NVIC_ENABLE_IRQ(SWI_IRQ_NUM);
#endif
#if defined(__DYN_SWI_DEBUG_LED__)
                pinMode(__DYN_SWI_DEBUG_LED__, OUTPUT);
                digitalWrite(__DYN_SWI_DEBUG_LED__, LOW);
#endif
                dyn_SWI_initied = 1;
        }
}

/**
 *
 * @param klass class that extends dyn_SWI
 * @return 0 on queue full, else returns queue position (ones based)
 */
int exec_SWI(const dyn_SWI* klass) {
        int rc = 0;

        uint8_t irestore = interruptsStatus();
        // Allow use from inside a critical section...
        // ... and prevent races if also used inside an ISR
        noInterrupts();
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) {
                if(!dyn_SWI_LIST[i]) {
                        rc = 1 + i; // Success!
                        dyn_SWI_LIST[i] = (dyn_SWI*)klass;
#if !defined(ARDUINO_spresense_ast)
                        if(!NVIC_GET_PENDING(SWI_IRQ_NUM)) NVIC_SET_PENDING(SWI_IRQ_NUM);
#else
                        // Launch 1-shot timer as an emulated SWI
                        // Hopefully the value of Zero is legal.
                        // 1 microsecond latency would suck!
                        attachTimerInterrupt(softISR, 100);
#endif
                        DDSB();
                        break;
                }
        }
        // Restore interrupts, if they were on.
        if(irestore) interrupts();
        return rc;
}
#elif defined(ARDUINO_ARCH_PIC32)

/**
 * Initialize the Dynamic (class) Software Interrupt
 */
static void Init_dyn_SWI(void) {
        if(!dyn_SWI_initied) {
                uint32_t sreg = disableInterrupts();

                setIntVector(SWI_VECTOR, softISR);
                setIntPriority(SWI_VECTOR, 1, 1); // Lowest priority, ever.
                ifs->clr = swibit;
                iec->clr = swibit;
                iec->set = swibit;
                restoreInterrupts(sreg);
#if defined(__DYN_SWI_DEBUG_LED__)
                pinMode(__DYN_SWI_DEBUG_LED__, OUTPUT);
                UHS_PIN_WRITE(__DYN_SWI_DEBUG_LED__, LOW);
#endif
        }
}

/**
 *
 * @param klass class that extends dyn_SWI
 * @return 0 on queue full, else returns queue position (ones based)
 */
int exec_SWI(const dyn_SWI* klass) {
        int rc = 0;
        uint32_t sreg = disableInterrupts();
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) {
                if(!dyn_SWI_LIST[i]) {
                        rc = 1 + i; // Success!
                        dyn_SWI_LIST[i] = (dyn_SWI*)klass;
                        if(!(ifs->reg & swibit)) ifs->set = swibit;
                        ;
                        break;
                }
        }
        restoreInterrupts(sreg);
        return rc;
}

#endif /* defined(__arm__) */
#endif	/* SWI_INLINE_H */
#else
#error "Never include SWI_INLINE.h directly, include dyn_SWI.h instead"
#endif
