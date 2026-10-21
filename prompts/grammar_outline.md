# Grammar reference outline

The sections of the grammar reference that `explain_sentence.md` sends
Claude, in order. Each becomes a `<section id="…" title="…">` block, merged
from the chapter notes in `grammar/notes/` (see the "Grammar reference" item
in `TODO.md`); the reference itself stays out of git, this outline doesn't.

Explanations cite these ids and are cached in the database, so an id must
never be renamed or reused for something else once explanations exist. Add
new ids instead, and retire old ones by leaving them in place.

One entry per section, on two lines:

    - `id` — Title. What the section covers.
      Sources: chapter notes (with the book's sections where it helps).

## Writing and sounds

- `script.alphabet` — The alphabet. Letters with their romanization; Arabic letters kept in loanwords; ع; doubled consonants; letter shapes.
  Sources: thackston-phonology (Writing System), kim-01-02 (ch. 1), swedish-01 (1.1)
- `script.vowels` — Writing vowels. ە vs ه, و/وو, ی/ێ, ۆ; the unwritten short i; ئ before a word-initial vowel; ی inserted between vowels.
  Sources: thackston-phonology, kim-01-02, kim-03 (inserted ی), swedish-01 (1.1)
- `script.spelling` — Spelling conventions. Orthographic peculiarities, which elements are written joined or apart, irregular spellings such as ye.
  Sources: thackston-37-42 (§ 41), thackston-phonology
- `sound.inventory` — Vowels and consonants. The sound system, including trilled ř and dark ł.
  Sources: thackston-phonology, kim-01-02, swedish-01
- `sound.stress` — Stress. Final stress on nouns and adjectives; stressed prefixes and stems in verbs.
  Sources: thackston-phonology (Stress)
- `sound.contractions` — Vowel contractions. Sequences that contract in speech and writing (-îa-, …).
  Sources: thackston-37-42 (§ 42)

## Nouns

- `noun.absolute` — The absolute state. The bare noun: generic sense, after numbers and classifiers.
  Sources: thackston-01-09 (§ 1), kim-03, swedish-01 (2.1)
- `noun.indefinite` — The indefinite state. -êk / -yek, indefinite plural -ân; quantifiers that take an indefinite noun (هەموو, چەند, هەر, چ).
  Sources: thackston-01-09 (§ 2), kim-03, swedish-01 (2.1)
- `noun.definite` — The definite state. -aka / -ka, plural -akân; forms after vowels.
  Sources: thackston-01-09 (§ 3), kim-03, swedish-01 (2.1)
- `noun.plural` — Plurals. -ân vs -akân; plurals of loanwords; number + -ân (دەیان 'tens of'); time word + -âna (ڕۆژانە 'daily').
  Sources: thackston-01-09, kim-03, swedish-01 (2.1.5)
- `noun.gender-case` — Gender and case. What remains of them: no grammatical gender, the vocative, the izâfa as genitive.
  Sources: swedish-01 (2.2–2.3)
- `noun.demonstrative` — Demonstratives. ئەم / ئەو … -a enveloping the noun; demonstrative pronouns ئەمە, ئەوە, ئەمانە, ئەوانە.
  Sources: thackston-01-09 (§ 4), kim-03, swedish-02 (2.3)
- `noun.compound` — Compound nouns and derivation. Attribute + noun compounds (پیرەژن); nouns, adjectives and adverbs formed with suffixes.
  Sources: swedish-02 (1.4), kim-04-07 (ch. 6)

## The izâfa (ezafe)

- `ezafe.open` — The open izâfa. Noun + -î + adjective or noun ('of'), with indefinite nouns; chains of izâfas.
  Sources: thackston-01-09 (§§ 5–6), kim-03, swedish-02 (1.1)
- `ezafe.close` — The close izâfa. -a- linking noun and adjective under a demonstrative or the definite suffix (کچە کوردەکە); nouns ending in -a.
  Sources: thackston-01-09 (§ 7), kim-03, swedish-02 (1.2)
- `ezafe.definite` — The definite suffix with adjectives. Where -aka goes in a noun–adjective phrase, and the difference in connotation.
  Sources: thackston-01-09 (§§ 8–9), kim-03
- `ezafe.multiple` — Several modifiers. Two or more adjectives (izâfa or و); mixing close and open izâfa.
  Sources: thackston-01-09 (§ 7), swedish-02 (1.3)

## Adjectives, numbers and particles

- `adjective.comparison` — Comparison. Comparative -tir with لە, superlative -tirîn and هەرە.
  Sources: thackston-10-14 (§ 13), kim-04-07 (ch. 6), swedish-02 (1.5)
- `number.cardinal` — Cardinal numbers. The numerals and fractions; classifiers (کەس, سەر, دانە); time words after numbers.
  Sources: thackston-10-14 (§ 12), kim-03, swedish-03
- `number.ordinal` — Ordinal numbers. -am / -amîn; ordinals in an izâfa (ساڵی دەیەم).
  Sources: thackston-10-14 (§ 12.1), kim-03, swedish-03
- `number.calendar` — Days and months. Days of the week, months, the Kurdish calendar.
  Sources: thackston-10-14 (§§ 12.2–12.3)
- `particle.ish` — Enclitic -îsh / -sh. 'Too, also, even'; its place before a possessive or agent clitic.
  Sources: thackston-10-14 (§ 11), kim-03, thackston-27 (§ 27)
- `particle.interjections` — Interjections. Common interjections and what each expresses.
  Sources: swedish-06
- `adverb` — Adverbs. Adverbs of time, place and manner, and how they are formed.
  Sources: swedish-04, kim-04-07 (ch. 6)

## Pronouns

- `pronoun.personal` — Independent personal pronouns. One form for subject and object; usually omitted as subject; dialect forms.
  Sources: thackston-10-14 (§ 10), kim-03, swedish-02 (2.1)
- `pronoun.possessive` — Independent possessives. هی + pronoun ('mine'); noun + izâfa + pronoun (کتێبی من).
  Sources: thackston-10-14 (§ 10.1), kim-03
- `pronoun.reflexive` — Reflexive خۆ. خۆ + clitic ('myself'), and as 'own'.
  Sources: kim-03, swedish-02 (2.5)
- `pronoun.interrogative` — Question words. چی, کێ, کام, کەی, چەند, کوێ and forms built on them.
  Sources: kim-04-07 (ch. 5), swedish-02 (2.4)
- `pronoun.reciprocal` — Reciprocal pronouns. یەک, یەکتر, یەکدی 'each other'.
  Sources: swedish-02 (2.7)

## Pronominal clitics

The clitics -im, -it, -î, -mân, -tân, -yân and the verbal endings. These
sections say, for each use, which set appears and what it attaches to.

- `clitics.sets` — The two sets. Set I (verbal personal endings) and Set II (-im -it -î -mân -tân -yân); an overview of which set appears where, by tense and transitivity.
  Sources: kim-03, swedish-02 (2.2), thackston-10-14 (§ 10.1)
- `clitics.possessive` — Clitics as possessors. On the definite form, or the absolute form for names, parents, homes and body parts; the rhetorical -akam.
  Sources: thackston-10-14 (§ 10.1), kim-03, swedish-02 (2.2)
- `clitics.object` — Clitics as objects of present-tense verbs. Placed on the da- / na- / bi- prefix or a preverb, not at the end of the verb.
  Sources: thackston-20-26 (§ 23), kim-03, kim-08-09-present, swedish-02 (2.2)
- `clitics.prepositional` — Preposed prepositional complements. A clitic before its preposition (-mân le, pêy, boy); the forms pê, lê, tê.
  Sources: thackston-10-14 (§ 14.1), kim-04-07 (ch. 7), kim-12, swedish-05
- `clitics.agent` — Clitics as agents of past transitive verbs. The ergative construction: the agent clitic on the first available element before the verb (negation, aspect prefix, preverb, object, prepositional phrase), what it can't attach to, one per verb; its loss in South Sorani.
  Sources: thackston-27 (§§ 27–27.1), kim-09-past, swedish-04
- `clitics.past-object` — Objects of past transitive verbs. Verbal endings as logical objects; the full agent × object table; agreement with the object.
  Sources: thackston-27 (§ 27.2), kim-09-past, swedish-04
- `clitics.bumping` — Bumped clitics. An agent clitic pushing a prepositional complement to the end of the verb, and a prepositional complement pushing a possessor there.
  Sources: thackston-27 (§§ 27.3–27.4)
- `clitics.experiencer` — Clitics as experiencers. Verbs of liking, loving, needing, forgetting and remembering (پێ خۆش بوون, حەز لێ بوون, خۆش ویستن, پێویست بوون, لەبیر چوون), where a clitic marks who likes, needs or forgets, in every tense.
  Sources: kim-10-11 (ch. 10), thackston-20-26 (§ 22)

## Verbs: forms and types

- `verb.stems` — Infinitive and stems. Past stem from the infinitive; the present stem and how to find it; irregular stems.
  Sources: kim-08-09-present (ch. 9), swedish-03 (2.1–2.3), kim-10-11
- `verb.endings` — Personal endings. Present endings after consonant and vowel stems; intransitive past endings.
  Sources: thackston-15-19 (§ 17), thackston-20-26 (§ 25), kim-08-09-present, swedish-03 (2.4)
- `verb.types` — Verb types. Transitive and intransitive; simple, complex and compound verbs.
  Sources: kim-08-09-present (ch. 8)
- `verb.compound` — Compound verbs. Noun or adjective + verb (قسە کردن); preverbs هەڵ-, دا-, ڕا-, وەر-, تێ-; where prefixes and clitics go in them.
  Sources: kim-08-09-present, thackston-27 (§ 27), swedish-week10 (2.2)
- `verb.awa` — Verbs in -awa. Meanings of the suffix and where it goes after the endings.
  Sources: thackston-15-19 (§ 18), kim-09-past
- `verb.postposed` — Postposed complements and directional -è. The -a / -è goal suffix on verbs of motion (چوومە ماڵ); directional nouns.
  Sources: thackston-34-36 (§ 35), thackston-37-42 (§ 40), kim-12
- `verb.copula` — The copula. Present enclitic forms of 'to be' after consonants and vowels; negative نی-; past and other forms of بوون.
  Sources: thackston-15-19 (§ 15), kim-04-07 (ch. 4), swedish-02 (3)
- `verb.have` — 'To have'. هەبوون with possessor clitics; negative نییە.
  Sources: thackston-15-19 (§ 16), kim-10-11 (ch. 10)
- `verb.want` — 'To want'. ویستن, its present and past constructions.
  Sources: thackston-20-26 (§ 20), kim-10-11 (ch. 10), swedish-week10
- `verb.can` — 'To be able'. توانین + subjunctive; other modal verbs and adjectives.
  Sources: thackston-20-26 (§ 21), kim-13, swedish-week10
- `verb.remember` — 'To remember' and 'to forget'. The idioms with بیر and their clitics.
  Sources: thackston-20-26 (§ 22), kim-10-11 (ch. 10)
- `verb.must` — Obligation. 'Must' with an invariable word + subjunctive.
  Sources: swedish-week10 (3.4)
- `verb.factitive` — Factitive verbs. The -ândin causative of intransitives.
  Sources: thackston-34-36 (§ 36), kim-10-11 (ch. 11)
- `verb.passive` — The passive. Passive stems -râ- / -rê-, their tenses, the past passive participle.
  Sources: thackston-34-36 (§§ 34–34.1), kim-10-11 (ch. 11), swedish-07

## Tenses and moods

- `tense.overview` — Synopsis of tenses and moods. Every tense and mood side by side, intransitive and transitive.
  Sources: thackston-tenses
- `tense.present` — The present. Habitual / progressive / future with da- + endings; negative nâ-; vowel stems; هاتن.
  Sources: thackston-15-19 (§ 17), kim-08-09-present, swedish-03 (2.5), swedish-week10
- `tense.subjunctive` — The present subjunctive. bi- + endings, negative na-; compound verbs without bi-; its uses (wishes, purpose, after modal verbs, با 'let's').
  Sources: thackston-15-19 (§ 19), kim-13, swedish-03 (2.6), swedish-week10
- `tense.imperative` — The imperative. Singular and plural, negative ma-, with objects.
  Sources: thackston-20-26 (§ 24), kim-08-09-present, swedish-03 (2.7), swedish-week10
- `tense.past` — The simple past. Intransitive: past stem + endings; transitive: past stem + agent clitic (see `clitics.agent`); negative na-.
  Sources: thackston-20-26 (§ 25), thackston-27 (§ 27), kim-09-past, swedish-04
- `tense.past-habitual` — The past habitual / progressive. da- + past stem; intransitive and transitive; negative.
  Sources: thackston-20-26 (§ 26), kim-09-past, swedish-04 (imperfekt)
- `tense.perfect` — The present perfect. The perfect participle -û / -w; intransitive and transitive present perfect.
  Sources: thackston-28-33 (§§ 28–29.1), kim-09-past, swedish-05
- `tense.past-perfect` — The past perfect. Participle + بوو; intransitive and transitive.
  Sources: thackston-28-33 (§§ 30–30.1), kim-09-past, swedish-06
- `tense.past-subjunctive` — The past subjunctive. Forms and uses.
  Sources: thackston-28-33 (§ 31), kim-13
- `tense.irrealis` — The irrealis. Unfulfilled wishes and conditions.
  Sources: thackston-28-33 (§ 32), kim-13
- `tense.past-conditional` — The past conditional. Its forms and their use in unreal conditions about the past.
  Sources: thackston-28-33 (§ 33)

## Sentences

- `syntax.word-order` — Word order. Subject–object–verb; parts of the sentence; where adverbs and prepositional phrases go.
  Sources: kim-01-02 (ch. 2), kim-04-07 (ch. 6)
- `syntax.negation` — Negation. Which prefix negates what: nâ- (present), na- (past, subjunctive), ma- (imperative), nî- (copula).
  Sources: thackston-15-19, thackston-20-26, kim-08-09-present, kim-09-past, swedish-03, swedish-04
- `syntax.questions` — Questions. Yes/no questions with ئایا or intonation; question-word questions.
  Sources: kim-04-07 (ch. 5), swedish-02 (2.4)
- `prep.simple` — Prepositions. بە, لە, بۆ, لەگەڵ, بێ …; their combined forms with pronouns.
  Sources: thackston-10-14 (§ 14), kim-04-07 (ch. 7), kim-12, swedish-05
- `prep.circum` — Postpositions and circumpositions. -dâ, -awa, -râ and لە … دا, بە … دا, لە … ەوە.
  Sources: thackston-10-14 (§ 14), kim-12, swedish-05 (2.3–2.4)
- `prep.compound` — Compound prepositions. Preposition + noun + izâfa (لەسەر, لەژێر, بەرامبەر …).
  Sources: swedish-05 (2.5), kim-12
- `syntax.conjunctions` — Conjunctions. Coordinating و, یان, بەڵام …; subordinating conjunctions and the mood they take.
  Sources: thackston-37-42 (§ 38), kim-14, swedish-06
- `syntax.relative` — Relative clauses. کە, the -ay / -î on the head noun, resumptive clitics.
  Sources: thackston-37-42 (§ 39), kim-14, swedish-02 (2.6)
- `syntax.conditional` — Conditional sentences. ئەگەر / گەر; real, possible and unreal conditions and their tenses.
  Sources: thackston-tenses (Conditional Sentence Types), thackston-28-33 (§§ 32–33), kim-13, swedish-week10 (3.2)
- `syntax.duration` — Expressions of time. 'For' and 'since' with statives and actions.
  Sources: thackston-37-42 (§ 37), kim-12
