// cond(code): canonical 2-letter x86 condition code (emitter maps every
// Jcc/CMOVcc/SETcc mnemonic first: jna->be, jae->nc, jnge->l, ...).
// Signedness is the opcode's: l/ge/le/g read SF/OF, a/be read CF/ZF,
// c=CF, p=PF (the UCOMISS NaN path sets ZF=PF=CF=1).
inline bool PesState::cond(const char* cc) const {
  if (!cc[1]) {
    switch (cc[0]) {
      case 'z':
      case 'e':
        return zf;
      case 'c':
      case 'b':
        return cf;
      case 'a':
        return (!cf && !zf);
      case 'l':
        return (sf != of);
      case 'g':
        return (!zf && (sf == of));
      case 's':
        return sf;
      case 'o':
        return of;
      case 'p':
        return pf;
      default:
        return false;
    }
  }
  switch (cc[0]) {
    case 'n':
      switch (cc[1]) {
        case 'z':
        case 'e':
          return !zf;
        case 'c':
          return !cf;
        case 'a':  // na = be
          return (cf || zf);
        case 'l':  // nl = ge
          return (sf == of);
        case 'g':  // ng = le
          return (zf || (sf != of));
        case 's':
          return !sf;
        case 'o':
          return !of;
        case 'p':
          return !pf;
        case 'b':  // nbe = a
          return (!cf && !zf);
        default:
          return false;
      }
    case 'b':
      return (cf || zf);  // be (b alone never reaches here: mapped to c)
    case 'a':
      return !cf;  // ae (a alone never reaches here: mapped to a)
    case 'g':
      return (sf == of);  // ge
    case 'l':
      return (zf || (sf != of));  // le
    case 'e':
      return zf;
    case 's':
      return sf;
    default:
      return false;
  }
}
