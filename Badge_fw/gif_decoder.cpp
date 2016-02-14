/*
 * gif_decoder.c
 *
 *  Created on: 13 февр. 2016 г.
 *      Author: Kreyl
 */

#include "gif_decoder.h"
#include "kl_lib.h"
#include "uart.h"

#if 1 // ==== External functions ====
// Returns byte if or and something negative if not ok
extern int16_t GetByte();
extern void DrawLine(uint8_t *Ptr, uint32_t Sz);
#endif

#if 1 // ==== Local variables ====
static int16_t curr_size;           // The current code size
static int16_t clear;               // Value for a clear code
static int16_t ending;              // Value for a ending code
static int16_t newcodes;            // First available code
static int16_t top_slot;            // Highest code for current size
static int16_t slot;                // Last read code

// The following static variables are used for seperating out codes
static int16_t navail_bytes = 0;    // # bytes left in block
static int16_t nbits_left = 0;      // # bits left in current byte
static uint8_t b1;                  // Current byte
static uint8_t byte_buff[257];      // Current block
static uint8_t *pbytes;             // Pointer to next byte in block

static int32_t code_mask[13] = {
     0,
     0x0001, 0x0003,
     0x0007, 0x000F,
     0x001F, 0x003F,
     0x007F, 0x00FF,
     0x01FF, 0x03FF,
     0x07FF, 0x0FFF
};

// Pixel line buffer
static uint8_t LineBuf[LINE_SZ];

/* The reason we have these seperated like this instead of using
 * a structure like the original Wilhite code did, is because this
 * stuff generally produces significantly faster code when compiled...
 * This code is full of similar speedups...  (For a good book on writing
 * C for speed or for space optimisation, see Efficient C by Tom Plum,
 * published by Plum-Hall Associates...)
 */

#define MAX_CODES   1023
static uint8_t stack[MAX_CODES + 1];            /* Stack for storing pixels */
static uint8_t suffix[MAX_CODES + 1];           /* Suffix table */
static uint16_t prefix[MAX_CODES + 1];           /* Prefix linked list */

#endif

static void Init(int16_t size) {
    curr_size = size + 1;
    top_slot = 1 << curr_size;
    clear = 1 << size;
    ending = clear + 1;
    slot = newcodes = ending + 1;
    navail_bytes = nbits_left = 0;
}


static inline int16_t CheckAvailBytes() {
    int16_t x;
    if(navail_bytes <= 0) {
        // Out of bytes in current block, so read next block
        pbytes = byte_buff;
        if((navail_bytes = GetByte()) < 0) return(navail_bytes);
        else if(navail_bytes) {
            for(int16_t i = 0; i < navail_bytes; ++i) {
                if((x = GetByte()) < 0) return(x);
                else byte_buff[i] = x;
            }
        }
    } // if(navail_bytes <= 0)
    b1 = *pbytes++;
    return 0;
}

/* get_next_code()
 * - gets the next code from the GIF file.  Returns the code, or else
 * a negative number in case of file errors. */
static int16_t get_next_code() {
    int16_t x;
    int32_t ret;

    if(nbits_left == 0) {
        if((x = CheckAvailBytes()) < 0) return x;
        nbits_left = 8;
        --navail_bytes;
    }

    ret = b1 >> (8 - nbits_left);
    while(curr_size > nbits_left) {
        if((x = CheckAvailBytes()) < 0) return x;
        ret |= b1 << nbits_left;
        nbits_left += 8;
        --navail_bytes;
    }
    nbits_left -= curr_size;
    ret &= code_mask[curr_size];
    return (int16_t)ret;
}


// Call when ImageData is ready to be read
uint8_t Decoder() {
    // Read code sz
    int16_t size = GetByte();
    if(size < 2 or size > 9) {
        Uart.Printf("Bad Code Sz\r");
        return FAILURE;
    }

    Init(size);

    int16_t oc=0, fc=0;
    // Set up the stack pointer and decode buffer pointer
    uint8_t *sp = stack;
    uint8_t *bufptr = LineBuf;
    int16_t bufcnt = LINE_SZ;

    /* This is the main loop.  For each code we get we pass through the
     * linked list of prefix codes, pushing the corresponding "character" for
     * each code onto the stack.  When the list reaches a single "character"
     * we push that on the stack too, and then start unstacking each
     * character for output in the correct order.  Special handling is
     * included for the clear code, and the whole thing ends when we get
     * an ending code.  */

    int16_t c, ret;
    while((c = get_next_code()) != ending) {
        if(c < 0) {
            Uart.Printf("GetNext1 Fail\r");
            return FAILURE;
        }
        // If the code is a clear code, reinitialize all necessary items
        if(c == clear) {
            curr_size = size + 1;
            slot = newcodes;
            top_slot = 1 << curr_size;
            // Continue reading codes until we get a non-clear code (Another unlikely, but possible case...)
            while((c = get_next_code()) == clear);
            /* If we get an ending code immediately after a clear code
            * (Yet another unlikely case), then break out of the loop. */
            if(c == ending) {
                Uart.Printf("end1\r");
                break;
            }
            /* Finally, if the code is beyond the range of already set codes,
            * (This one had better NOT happen...  I have no idea what will
            * result from this, but I doubt it will look good...) then set it
            * to color zero. */
            if (c >= slot) c = 0;
            oc = fc = c;

            /* And let us not forget to put the char into the buffer... And
            * if, on the off chance, we were exactly one pixel from the end
            * of the line, we have to send the buffer to the out_line()
            * routine. */
            *bufptr++ = c;
            if(--bufcnt == 0) {
                DrawLine(LineBuf, LINE_SZ);
                bufptr = LineBuf;
                bufcnt = LINE_SZ;
            }
        } // if(c == clear)
        else {
            /* In this case, it's not a clear code or an ending code, so
            * it must be a code code...  So we can now decode the code into
            * a stack of character codes. (Clear as mud, right?) */
            int16_t code = c;

            /* Here we go again with one of those off chances...  If, on the
            * off chance, the code we got is beyond the range of those already
            * set up (Another thing which had better NOT happen...) we trick
            * the decoder into thinking it actually got the last code read.
            * (Hmmn... I'm not sure why this works...  But it does...) */
            if(code >= slot) {
                code = oc;
                *sp++ = fc;
            }

            /* Here we scan back along the linked list of prefixes, pushing
            * helpless characters (ie. suffixes) onto the stack as we do so. */
            while(code >= newcodes) {
                *sp++ = suffix[code];
                code = prefix[code];
            }

            /* Push the last character on the stack, and set up the new
            * prefix and suffix, and if the required slot number is greater
            * than that allowed by the current bit size, increase the bit
            * size.  (NOTE - If we are all full, we *don't* save the new
            * suffix and prefix...  I'm not certain if this is correct...
            * it might be more proper to overwrite the last code...  */
            *sp++ = code;
            if(slot < top_slot) {
                suffix[slot] = fc = code;
                prefix[slot++] = oc;
                oc = c;
            }
            if(slot >= top_slot) {
                if (curr_size < 12) {
                    top_slot <<= 1;
                    ++curr_size;
                }
            }

            /* Now that we've pushed the decoded string (in reverse order)
            * onto the stack, lets pop it off and put it into our decode
            * buffer...  And when the decode buffer is full, write another
            * line... */
            while(sp > stack) {
                *bufptr++ = *(--sp);
                if(--bufcnt == 0) {
                    DrawLine(LineBuf, LINE_SZ);
                    bufptr = LineBuf;
                    bufcnt = LINE_SZ;
                }
            } // while(sp > stack)
        } // // if(c != clear)
    } // while get next code
    ret = 0;
    if(bufcnt != LINE_SZ) DrawLine(LineBuf, (LINE_SZ - bufcnt));
    Uart.Printf("Done\r");
    return ret;
}

