



So I want a PL/I-like MACRODEF for ?PROLOGUE which generates module entry code.  I want it to branch over a module ID string; it can do this using GEN DO assembler.  It would be nice if it could extract the module name from the compiler environment, but if it cannot then ?PROLOGUE EYE('MODNAME'); would provide the name to set the bytes to.  the machine code would be a branch to a label past the M O D N A M E bytes.  Makes sense?  Can you show me the MACRODEF code for this

Yes — that makes sense.

Restating the goal
You want a PL/I-like MACRODEF named ?PROLOGUE that:

emits a branch at module entry,

places a module “eye-catcher” string immediately after that branch,

lands at a label just past the bytes of the string,

preferably derives the module name automatically from the compilation environment,

but can fall back to an explicit argument such as:

?PROLOGUE EYE('MODNAME');
So the generated layout is conceptually:

ENTRY:
    b   AFTER_EYE
EYE:
    db  'M','O','D','N','A','M','E'
AFTER_EYE:
    ; normal module code starts here
That is a sound design. It is simple, position-independent, and gives the loader/debugger/emulator a recognizable module signature near the entry point.

High-level design
The macro should do three things:

1. Choose the eye-catcher text
Either:

use EYE('MODNAME'), or

use some compiler-provided symbol like current module/file name, if your macro processor exposes one.

2. Generate unique local labels
You need at least:

one label for the start of the eye-catcher area

one label for the “past it” target

3. Emit inline assembler with GEN DO
That assembler will:

branch to the end label,

emit the string bytes,

define the continuation label.

Main practical constraint
Because this is a macro facility, not a full compiler pass, the hard part is usually not the GEN DO itself — it is:

string handling,

optional argument handling,

unique local label generation,

and whether the macro processor can inspect the current compilation unit name.

So I’ll show this in a PL/I-like pseudo-MACRODEF style that should be straightforward to adapt to your PL/SW macro syntax.

Version 1: explicit EYE(...) only
This is the simplest and most robust form.

MACRODEF ?PROLOGUE(EYE);
   DECLARE %EYE CHARACTER;
   %EYE = EYE;

   /* Generate unique labels if your macro system supports this. */
   DECLARE %L_SKIP CHARACTER;
   DECLARE %L_EYE  CHARACTER;

   %L_SKIP = '%PROLOGUE_SKIP';
   %L_EYE  = '%PROLOGUE_EYE';

   GEN DO;
%L_EYE:
       B    %L_SKIP
       DC   C'%EYE'
%L_SKIP:
   END;
MEND;
Used like this:

?PROLOGUE('MODNAME');
or, if your macro syntax wants keywords:

?PROLOGUE EYE('MODNAME');
Depending on your macro formal parameter syntax, the header may need to be one of:

MACRODEF ?PROLOGUE(EYE);
or:

MACRODEF ?PROLOGUE;
   /* parse keyword EYE(...) */
Version 2: keyword-style EYE('MODNAME')
If you want the call form to look like classic PL/I / PL/SW macro invocation style:

?PROLOGUE EYE('MODNAME');
then conceptually you want something more like this:

MACRODEF ?PROLOGUE;
   DECLARE %EYE CHARACTER;
   DECLARE %L_SKIP CHARACTER;
   DECLARE %L_EYE  CHARACTER;

   /* Default if keyword omitted */
   %EYE = 'NONAME';

   /* If your macro processor supports keyword operands */
   %IF DEFINED(EYE) %THEN
      %EYE = EYE;

   %L_SKIP = '%PROLOGUE_SKIP';
   %L_EYE  = '%PROLOGUE_EYE';

   GEN DO;
%L_EYE:
       B    %L_SKIP
       DC   C'%EYE'
%L_SKIP:
   END;
MEND;
Invocation:

?PROLOGUE EYE('MODNAME');
Version 3: branch first, then eye-catcher
The layout you described specifically says:

branch over a module ID string

So the actual order should be:

branch instruction first

bytes of eye-catcher second

continuation label third

That gives:

MACRODEF ?PROLOGUE(EYE);
   DECLARE %EYE CHARACTER;
   DECLARE %L_SKIP CHARACTER;

   %EYE = EYE;
   %L_SKIP = '%PROLOGUE_SKIP';

   GEN DO;
       B    %L_SKIP
       DC   C'%EYE'
%L_SKIP:
   END;
MEND;
This is probably the closest to what you want.

Used as:

?PROLOGUE('MODNAME');
or adapted to:

?PROLOGUE EYE('MODNAME');
Better version: fixed-size eye-catcher field
Architecturally, I would strongly recommend a fixed-length module ID field, for example 8 or 16 bytes.

Why this is better
Benefits

predictable scanning by loader/debugger/emulator

easy dump analysis

easier tooling later

no need to parse variable-length structures at entry

Risks of variable-length

need to know where the string ends

harder for post-processing tools

easier to break if macro quoting changes

A fixed-size version might emit exactly 8 bytes, padded with blanks.

Conceptual generated code
ENTRY:
    b   AFTER_EYE
    dc  c'MODNAME '   ; padded to 8
AFTER_EYE:
Macro sketch
MACRODEF ?PROLOGUE(EYE);
   DECLARE %EYE CHARACTER;
   DECLARE %PAD CHARACTER;
   DECLARE %L_SKIP CHARACTER;

   %EYE = EYE;

   /* pad/truncate to 8 chars - exact functions depend on your macro language */
   %PAD = SUBSTR(%EYE || '        ', 1, 8);

   %L_SKIP = '%PROLOGUE_SKIP';

   GEN DO;
       B    %L_SKIP
       DC   C'%PAD'
%L_SKIP:
   END;
MEND;
If your macro language lacks SUBSTR at macro-expansion time, you can still require the caller to provide an already-sized constant:

?PROLOGUE EYE('MODNAME');
?PROLOGUE EYE('EDITOR  ');
?PROLOGUE EYE('APL     ');
If you want a length byte too
Another strong option is:

    b   AFTER_EYE
    dc  al1(7)
    dc  c'MODNAME'
AFTER_EYE:
Pros
variable-length supported cleanly

tools can read one byte and know exact length

Cons
slightly more complexity

one more convention to maintain

Macro sketch:

MACRODEF ?PROLOGUE(EYE);
   DECLARE %EYE CHARACTER;
   DECLARE %LEN CHARACTER;
   DECLARE %L_SKIP CHARACTER;

   %EYE = EYE;
   %LEN = LENGTH(%EYE);
   %L_SKIP = '%PROLOGUE_SKIP';

   GEN DO;
       B    %L_SKIP
       DC   AL1(%LEN)
       DC   C'%EYE'
%L_SKIP:
   END;
MEND;
This is often the nicest compromise if your assembler supports AL1() or equivalent.

Auto-deriving module name from compiler environment
This is the part that is toolchain-specific.

Best-case
If your compiler or macro processor exposes something like:

current source filename

current compilation unit name

current procedure name

current external symbol

then the macro can default from that:

MACRODEF ?PROLOGUE(EYE);
   DECLARE %EYEVAL CHARACTER;

   %IF DEFINED(EYE) %THEN
      %EYEVAL = EYE;
   %ELSE
      %EYEVAL = %MODULE;   /* hypothetical compiler-supplied symbol */

   GEN DO;
       B    %PROLOGUE_SKIP
       DC   C'%EYEVAL'
%PROLOGUE_SKIP:
   END;
MEND;
More realistic
Most homemade macro systems do not expose that cleanly. In that case, the pragmatic answer is:

?PROLOGUE EYE('MODNAME');
That is probably the right v1.

My recommended v1 shape
Given your environment, I would start with this convention:

Call site
?PROLOGUE EYE('EDITOR');
Expansion intent
    b   .L_prologue_end_001
    dc  c'EDITOR  '      ; padded fixed-width, e.g. 8 bytes
.L_prologue_end_001:
Why this is the best first step
Benefits
trivial to implement

no dependence on hidden compiler state

easy for loader/emulator/debugger to inspect

naturally PIC-friendly

Risks
caller must keep module name in sync manually

Later improvement
Add a helper tool or compiler pass that auto-inserts the module name if omitted.

Concrete pseudo-PL/SW macro example
Here is the version I would hand you as the best practical starting point:

MACRODEF ?PROLOGUE(EYE);
   DECLARE %NAME CHARACTER;
   DECLARE %EMIT CHARACTER;
   DECLARE %LEND CHARACTER;

   %NAME = EYE;

   /* pad/truncate to 8 chars if macro functions allow it */
   %EMIT = SUBSTR(%NAME || '        ', 1, 8);

   /* unique label generation, replace with your macro facility's form */
   %LEND = %GENSYM('PROLOGUE_END');

   GEN DO;
       B    %LEND
       DC   C'%EMIT'
%LEND:
   END;
MEND;
Invocation:

?PROLOGUE('EDITOR');
?PROLOGUE('PASCAL');
?PROLOGUE('SHELL');
If you insist on the keyword form:

?PROLOGUE EYE('EDITOR');
then the same idea applies, just with whatever keyword-operand syntax your MACRODEF parser expects.

Even more concrete expansion example
For:

?PROLOGUE EYE('EDITOR');
I would expect something like:

       B    $PROLOGUE_END_17
       DC   C'EDITOR  '
$PROLOGUE_END_17:
And if your assembler does not support DC C'...', then byte-by-byte:

       B    $PROLOGUE_END_17
       DB   'E'
       DB   'D'
       DB   'I'
       DB   'T'
       DB   'O'
       DB   'R'
       DB   ' '
       DB   ' '
$PROLOGUE_END_17:
Low-level considerations
Branch encoding
This assumes your branch instruction can reach the local label with PC-relative displacement. That is normally fine for a few bytes.

Alignment
If later code expects alignment after entry, you may want:

    b   after_eye
    dc  c'EDITOR  '
    align 3
after_eye:
or equivalent.

External entry point
If the module entry symbol must be exported, the macro can also emit that label:

MODULE_ENTRY:
    b   after_eye
    dc  c'EDITOR  '
after_eye:
Multiple entry points
If you eventually support more than one entry point, keep the eye-catcher only at the primary entry.

Recommended evolution path
v1
Explicit eye string only.

v2
Fixed-width padded eye field.

v3
Optional auto-default from module name if compiler environment exposes it.

v4
Emit richer entry metadata, for example:

eye string

module version

flags

entry vector pointer

That would let your loader/debugger discover module capabilities.

Bottom line
Yes, the idea is good, and the generated structure is sensible.

The cleanest starting point is:

MACRODEF ?PROLOGUE(EYE);
   DECLARE %LEND CHARACTER;
   %LEND = %GENSYM('PROLOGUE_END');

   GEN DO;
       B    %LEND
       DC   C'EYE'
%LEND:
   END;
MEND;
but with EYE substituted correctly by your macro processor.

A more realistic useful version is:

MACRODEF ?PROLOGUE(EYE);
   DECLARE %NAME CHARACTER;
   DECLARE %EMIT CHARACTER;
   DECLARE %LEND CHARACTER;

   %NAME = EYE;
   %EMIT = SUBSTR(%NAME || '        ', 1, 8);
   %LEND = %GENSYM('PROLOGUE_END');

   GEN DO;
       B    %LEND
       DC   C'%EMIT'
%LEND:
   END;
MEND;
If you want, I can next do one of these two things:

write this in a more exact PL/I-style macro syntax, assuming classic keyword operands like EYE('MODNAME'), or

write it in a PL/SW-specific proposed macro syntax that includes GEN DO, local-label generation, and string padding rules.




