u_char next_sail7bit_code(u_char *p){
  u_char a,b,c;
  a = *p++;  
  switch(a){
  case 033:     return 0175;    // ESC into Stanford-ALT § ◊
  case 010:     return 0177;    // BS into rubout
  case 0136:    return 4;       // caret ^ into boolean AND ∧
  case 0137:    return 030;     // underbar
  case 0175:    return 0176;    // right curly
  case 0176:    return 032;     // tilde
  case 0177:    return 010;     // DEL into BS
    // two byte UTF-8
  case 0302:
  case 0316:
  case 0317:
    b = *p++;
    switch(b){
    case 0254: return   5;// ¬    
    case 0261: return   2;// α
    case 0262: return   3;// β
    case 0265: return   6;// ε
    case 0273: return 010;// λ    
    case 0200: return   7;// π
    default:   return   0;
    }    
    // three byte UTF-8    
  case 0342:    // first byte
    b = *p++;   // second byte
    c = *p++;   // third byte
    switch(b){
    case 0206:
      switch(c){
      case 0220:return 0137;// ←
      case 0221:return 0136;// ↑
      case 0222:return 031;// →
      case 0223:return 1;// ↓
      case 0224:return 027;// ↔
      default:  return 0;
      }
    case 0210:
      switch(c){
      case 0200: return 024;// ∀
      case 0202: return 017;// ∂
      case 0203: return 025;// ∃
      case 0236: return 016;// ∞
      case 0247: return 004;// ∧
      case 0250: return 037;// ∨
      case 0251: return 022;// ∩
      case 0252: return 023;// ∪
      default:   return 0;
      }
    case 0211:   
      switch(c){
      case 0240: return 033;// ≠
      case 0241: return 036;// ≡
      case 0244: return 034;// ≤
      case 0245: return 035;// ≥
      default:   return 0;
      }
    case 0212:   
      switch(c){
      case 0202: return 020;// ⊂
      case 0203: return 021;// ⊃
      default: return 0;
      }
    case 0223:
      switch(c){
      case 0247: return 026;// ⊗
      default: return 0;
      }
    }
  default:
    switch(a&0370){
    case 0340:
    case 0350: p+=2; return 0; // Ignore non SAIL three byte UTF-8 character.
    case 0360: p+=3; return 0; // Ignore non SAIL  four byte UTF-8 character.
    default:         return a; // all the other 7-bit codes are same as SAIL7
    }    
  }
  // return 0;
}
/*
  Octal code point <-> Octal UTF-8 conversion
  First code point    Last code point Byte1     Byte2   Byte3	Byte4
  000                 177             xxx	
  0200                3777            3xx	2xx	
  04000               77777           34x	2xx	2xx	
  100000              177777          35x	2xx	2xx	
  0200000             4177777         36x	2xx	2xx	2xx
  ====================================================================
  ASCII        Stanford 7-bit code    glyph
  ====================================================================
  033          0175   ESC
  010          0177   BS
  0136          004   ^ caret ( not really ∧ Boolean AND at 004 )
  0137          030   _        ASCII   _   0137   Stanford   _   030
  0175         0176   }        ASCII   }   0175   Stanford   }  0176
  0176          032   ~        ASCII tilde 0176   Stanford tilde 032
  0177          010            ASCII rubout       Stanford backspace
  ====================================================================
  UTF-8-as-octal      Stanford-7-bit-code          glyph
  ====================================================================
  \302\254              005                        ¬
  \316\261              002                        α
  \316\262              003                        β
  \316\265              006                        ε
  \316\273              010                        λ
  \317\200              007                        π

  \342\206\220          137                        ←
  \342\206\221          136                        ↑
  \342\206\222          031                        →
  \342\206\223          001                        ↓
  \342\206\224          027                        ↔

  \342\210\200          024                        ∀
  \342\210\202          017                        ∂
  \342\210\203          025                        ∃
  \342\210\236          016                        ∞
  \342\210\247          004                        ∧
  \342\210\250          037                        ∨
  \342\210\251          022                        ∩
  \342\210\252          023                        ∪

  \342\211\240          033                        ≠
  \342\211\241          036                        ≡
  \342\211\244          034                        ≤
  \342\211\245          035                        ≥

  \342\212\202          020                        ⊂
  \342\212\203          021                        ⊃

  \342\223\247          026                        ⊗
*/
