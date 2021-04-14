union WORD_PDP10
{
  ulong fw; // 64-bit representation for the 36-bit KA10 full word
  mixin(bitfields!( // half words
                   uint,"r",18,
                   uint,"l",18,
                   uint,"",28)); 
  mixin(bitfields!( // SAIL sixbit bit text
                   uint,"sixbit6",6,
                   uint,"sixbit5",6,
                   uint,"sixbit4",6,
                   uint,"sixbit3",6,
                   uint,"sixbit2",6,
                   uint,"sixbit1",6,
                   uint, "", 28)); // pad into 64 bits
}
