// SPDX-License-Identifier: MIT

#include "haetae_ntt_mve.h"

#include <arm_mve.h>
#include <stdint.h>

#define HAETAE_Q 64513
#define HAETAE_DQ (2 * HAETAE_Q)

typedef struct {
  int32_t root;
  int32_t twisted;
} haetae_root_pair;

/*
 * Each pair contains a centered standard-domain root and
 * round(root * 2^31 / q). The source scalar table stores Montgomery roots;
 * these values were converted with R^-1 = 50386 mod q.
 *
 * The pqmx MVE Barrett product is:
 *   lo = vmul(x, root)
 *   quo = vqrdmulh(x, twisted)
 *   r = lo - quo*q
 * It is congruent to x*root mod q without a 64-bit vector multiply.
 */
static const haetae_root_pair haeta_roots[HAETAE_MVE_N]
    __attribute__((aligned(16))) = {
    {0, 0},
    {28837, 959914838},
    {16153, 537694780},
    {20201, 672443030},
    {17287, 575442931},
    {13268, 441660023},
    {24647, 820439748},
    {5818, 193667321},
    {-19924, -663222361},
    {4390, 146132612},
    {22985, 765115739},
    {11883, 395556681},
    {8719, 290234680},
    {22642, 753698088},
    {6128, 203986480},
    {12029, 400416673},
    {13297, 442625363},
    {-19683, -655200047},
    {22664, 754430416},
    {-19435, -646944720},
    {5420, 180418852},
    {-18459, -614456011},
    {5119, 170399281},
    {10859, 361470168},
    {25463, 847602439},
    {-10435, -347356221},
    {-31049, -1033547034},
    {15914, 529739041},
    {6682, 222427817},
    {-11497, -382707664},
    {4097, 136379342},
    {21886, 728532654},
    {-25359, -844140527},
    {-22628, -753232062},
    {-30890, -1028254304},
    {20574, 684859309},
    {-15198, -505905112},
    {-27917, -929290236},
    {-21329, -709991455},
    {2569, 85515873},
    {-13100, -436067704},
    {23428, 779862150},
    {-1660, -55257434},
    {-774, -25764611},
    {-19070, -634794742},
    {-12778, -425349093},
    {11865, 394957504},
    {-25947, -863713642},
    {10828, 360438252},
    {4116, 137011807},
    {9941, 330912141},
    {-27155, -903925077},
    {31423, 1045996600},
    {-4547, -151358767},
    {-12565, -418258832},
    {-31897, -1061774928},
    {-5600, -186410622},
    {-11161, -371523026},
    {-9574, -318695588},
    {30202, 1005352427},
    {26813, 892540714},
    {18176, 605035617},
    {-29893, -995066555},
    {-1735, -57754005},
    {-12063, -401548451},
    {-6635, -220863299},
    {-24379, -811518668},
    {-19062, -634528441},
    {-27065, -900929192},
    {4869, 162077378},
    {23656, 787451726},
    {7610, 253318720},
    {-32226, -1072726552},
    {8603, 286373317},
    {8819, 293563441},
    {3257, 108417749},
    {-21107, -702601605},
    {17596, 585728803},
    {9834, 327350367},
    {-16090, -535597661},
    {-22393, -745409473},
    {28189, 938344466},
    {10262, 341597464},
    {4163, 138576325},
    {-29791, -991671219},
    {-27959, -930688316},
    {-11556, -384671633},
    {-30727, -1022828423},
    {-13776, -458570129},
    {12542, 417493217},
    {-18391, -612192454},
    {20106, 669280707},
    {-28229, -939675971},
    {-14639, -487297337},
    {-5153, -171531059},
    {-23622, -786319947},
    {-15029, -500279506},
    {7061, 235043821},
    {-1018, -33886788},
    {-2651, -88245457},
    {-12472, -415163084},
    {4911, 163475458},
    {13883, 462131903},
    {-23607, -785820633},
    {-31550, -1050224127},
    {19489, 648742251},
    {25550, 850498461},
    {-17623, -586627569},
    {-11948, -397720376},
    {19457, 647677047},
    {26852, 893838930},
    {-18415, -612991356},
    {20661, 687755331},
    {23702, 788982956},
    {11384, 378946164},
    {-26249, -873766501},
    {22739, 756926986},
    {14411, 479707762},
    {30558, 1017202817},
    {17979, 598477958},
    {7689, 255948441},
    {-3488, -116107187},
    {12892, 429143881},
    {-21815, -726169234},
    {22963, 764383411},
    {22599, 752266721},
    {-28411, -945734316},
    {27093, 901861245},
    {426, 14180522},
    {27092, 901827957},
    {-21713, -722773897},
    {25397, 845405456},
    {9780, 325552836},
    {-24976, -831391372},
    {-15997, -532501913},
    {26974, 897900019},
    {28092, 935115568},
    {-737, -24532969},
    {-14366, -478209820},
    {30144, 1003421746},
    {-27460, -914077798},
    {-31458, -1047161667},
    {30008, 998894631},
    {27827, 926294351},
    {-12622, -420156226},
    {1732, 57654142},
    {-22086, -735190176},
    {-21646, -720543628},
    {-13548, -450980554},
    {7052, 234744233},
    {-12748, -424350465},
    {-19002, -632531184},
    {9054, 301386030},
    {6087, 202621688},
    {-1709, -56888527},
    {5499, 183048573},
    {7960, 264969384},
    {5266, 175292559},
    {3471, 115541298},
    {-30949, -1030218273},
    {-29263, -974095360},
    {-27091, -901794669},
    {1512, 50330868},
    {-9244, -307710676},
    {-23048, -767212858},
    {-22250, -740649345},
    {10179, 338834592},
    {-2327, -77460271},
    {32031, 1066235468},
    {-19187, -638689392},
    {2483, 82653138},
    {-7159, -238306007},
    {4818, 160379710},
    {-24336, -810087301},
    {22476, 748172345},
    {-21699, -722307871},
    {-32208, -1072127375},
    {11565, 384971221},
    {-22992, -765348752},
    {-20203, -672509605},
    {32007, 1065436565},
    {-1632, -54325381},
    {1889, 62880297},
    {24121, 802930465},
    {1381, 45970191},
    {19376, 644980751},
    {-14205, -472850514},
    {27965, 930888041},
    {3537, 117738280},
    {1416, 47135257},
    {-25357, -844073952},
    {-29467, -980886033},
    {22202, 739051539},
    {12062, 401515164},
    {1139, 37914589},
    {8226, 273823888},
    {18137, 603737401},
    {9778, 325486260},
    {13428, 446986040},
    {16210, 539592174},
    {12993, 432505930},
    {-12363, -411534735},
    {15140, 503974430},
    {-31804, -1058679180},
    {-24275, -808056757},
    {12388, 412366925},
    {-4061, -135180988},
    {-15962, -531336847},
    {8506, 283144419},
    {9096, 302784110},
    {-15272, -508368395},
    {31587, 1051455768},
    {18095, 602339321},
    {24371, 811252368},
    {-19868, -661358255},
    {6437, 214272352},
    {2107, 70136996},
    {-11687, -389032310},
    {-28493, -948463900},
    {-15073, -501744160},
    {-26136, -870005001},
    {21547, 717248154},
    {-1736, -57787293},
    {1056, 35151717},
    {-15567, -518188240},
    {-24125, -803063615},
    {17923, 596613852},
    {31908, 1062141092},
    {-23006, -765814779},
    {27670, 921068196},
    {-21038, -700304760},
    {7446, 247859551},
    {-21596, -718879247},
    {-19863, -661191817},
    {-18397, -612392179},
    {-23890, -795241027},
    {6679, 222327954},
    {31018, 1032515118},
    {20151, 670778649},
    {25796, 858687213},
    {27818, 925994763},
    {-31489, -1048193583},
    {11109, 369792071},
    {-21325, -709858304},
    {9864, 328348995},
    {10351, 344560061},
    {-13918, -463296970},
    {-17993, -598943985},
    {-14649, -487630213},
    {-2089, -69537819},
    {8387, 279183193},
    {-3318, -110448293},
    {-23738, -790181310},
    {14737, 490559523},
    {25358, 844107240},
    {-6209, -206682777}
};

typedef struct {
  int32_t root[4];
  int32_t twisted[4];
} haetae_root_vec4;

/*
 * Vector-only lower-layer twiddles. For forward len=4/2, lane j is
 * haeta_roots[32+4*b+j] / haeta_roots[64+4*b+j]. Forward len=1
 * splits indices 128+8*b+2*j and 129+8*b+2*j into even/odd vectors.
 * Inverse tables reverse those maps from 63, 127, and 255/254 and negate
 * both fields once, so full-width GS butterflies need no runtime sign fixup.
 * Values are the exact root/twisted pairs above, reordered only; the scalar
 * table remains the source for all upper layers and preserved variants.
 */
static const haetae_root_vec4 haeta_ct_len4_v4[8]
    __attribute__((aligned(16))) = {
    {{-25359, -22628, -30890, 20574}, {-844140527, -753232062, -1028254304, 684859309}},
    {{-15198, -27917, -21329, 2569}, {-505905112, -929290236, -709991455, 85515873}},
    {{-13100, 23428, -1660, -774}, {-436067704, 779862150, -55257434, -25764611}},
    {{-19070, -12778, 11865, -25947}, {-634794742, -425349093, 394957504, -863713642}},
    {{10828, 4116, 9941, -27155}, {360438252, 137011807, 330912141, -903925077}},
    {{31423, -4547, -12565, -31897}, {1045996600, -151358767, -418258832, -1061774928}},
    {{-5600, -11161, -9574, 30202}, {-186410622, -371523026, -318695588, 1005352427}},
    {{26813, 18176, -29893, -1735}, {892540714, 605035617, -995066555, -57754005}},
};

static const haetae_root_vec4 haeta_gs_len4_v4[8]
    __attribute__((aligned(16))) = {
    {{1735, 29893, -18176, -26813}, {57754005, 995066555, -605035617, -892540714}},
    {{-30202, 9574, 11161, 5600}, {-1005352427, 318695588, 371523026, 186410622}},
    {{31897, 12565, 4547, -31423}, {1061774928, 418258832, 151358767, -1045996600}},
    {{27155, -9941, -4116, -10828}, {903925077, -330912141, -137011807, -360438252}},
    {{25947, -11865, 12778, 19070}, {863713642, -394957504, 425349093, 634794742}},
    {{774, 1660, -23428, 13100}, {25764611, 55257434, -779862150, 436067704}},
    {{-2569, 21329, 27917, 15198}, {-85515873, 709991455, 929290236, 505905112}},
    {{-20574, 30890, 22628, 25359}, {-684859309, 1028254304, 753232062, 844140527}},
};

static const haetae_root_vec4 haeta_ct_len2_v4[16]
    __attribute__((aligned(16))) = {
    {{-12063, -6635, -24379, -19062}, {-401548451, -220863299, -811518668, -634528441}},
    {{-27065, 4869, 23656, 7610}, {-900929192, 162077378, 787451726, 253318720}},
    {{-32226, 8603, 8819, 3257}, {-1072726552, 286373317, 293563441, 108417749}},
    {{-21107, 17596, 9834, -16090}, {-702601605, 585728803, 327350367, -535597661}},
    {{-22393, 28189, 10262, 4163}, {-745409473, 938344466, 341597464, 138576325}},
    {{-29791, -27959, -11556, -30727}, {-991671219, -930688316, -384671633, -1022828423}},
    {{-13776, 12542, -18391, 20106}, {-458570129, 417493217, -612192454, 669280707}},
    {{-28229, -14639, -5153, -23622}, {-939675971, -487297337, -171531059, -786319947}},
    {{-15029, 7061, -1018, -2651}, {-500279506, 235043821, -33886788, -88245457}},
    {{-12472, 4911, 13883, -23607}, {-415163084, 163475458, 462131903, -785820633}},
    {{-31550, 19489, 25550, -17623}, {-1050224127, 648742251, 850498461, -586627569}},
    {{-11948, 19457, 26852, -18415}, {-397720376, 647677047, 893838930, -612991356}},
    {{20661, 23702, 11384, -26249}, {687755331, 788982956, 378946164, -873766501}},
    {{22739, 14411, 30558, 17979}, {756926986, 479707762, 1017202817, 598477958}},
    {{7689, -3488, 12892, -21815}, {255948441, -116107187, 429143881, -726169234}},
    {{22963, 22599, -28411, 27093}, {764383411, 752266721, -945734316, 901861245}},
};

static const haetae_root_vec4 haeta_gs_len2_v4[16]
    __attribute__((aligned(16))) = {
    {{-27093, 28411, -22599, -22963}, {-901861245, 945734316, -752266721, -764383411}},
    {{21815, -12892, 3488, -7689}, {726169234, -429143881, 116107187, -255948441}},
    {{-17979, -30558, -14411, -22739}, {-598477958, -1017202817, -479707762, -756926986}},
    {{26249, -11384, -23702, -20661}, {873766501, -378946164, -788982956, -687755331}},
    {{18415, -26852, -19457, 11948}, {612991356, -893838930, -647677047, 397720376}},
    {{17623, -25550, -19489, 31550}, {586627569, -850498461, -648742251, 1050224127}},
    {{23607, -13883, -4911, 12472}, {785820633, -462131903, -163475458, 415163084}},
    {{2651, 1018, -7061, 15029}, {88245457, 33886788, -235043821, 500279506}},
    {{23622, 5153, 14639, 28229}, {786319947, 171531059, 487297337, 939675971}},
    {{-20106, 18391, -12542, 13776}, {-669280707, 612192454, -417493217, 458570129}},
    {{30727, 11556, 27959, 29791}, {1022828423, 384671633, 930688316, 991671219}},
    {{-4163, -10262, -28189, 22393}, {-138576325, -341597464, -938344466, 745409473}},
    {{16090, -9834, -17596, 21107}, {535597661, -327350367, -585728803, 702601605}},
    {{-3257, -8819, -8603, 32226}, {-108417749, -293563441, -286373317, 1072726552}},
    {{-7610, -23656, -4869, 27065}, {-253318720, -787451726, -162077378, 900929192}},
    {{19062, 24379, 6635, 12063}, {634528441, 811518668, 220863299, 401548451}},
};

static const haetae_root_vec4 haeta_ct_len1_even_v4[16]
    __attribute__((aligned(16))) = {
    {{426, -21713, 9780, -15997}, {14180522, -722773897, 325552836, -532501913}},
    {{28092, -14366, -27460, 30008}, {935115568, -478209820, -914077798, 998894631}},
    {{-12622, -22086, -13548, -12748}, {-420156226, -735190176, -450980554, -424350465}},
    {{9054, -1709, 7960, 3471}, {301386030, -56888527, 264969384, 115541298}},
    {{-29263, 1512, -23048, 10179}, {-974095360, 50330868, -767212858, 338834592}},
    {{32031, 2483, 4818, 22476}, {1066235468, 82653138, 160379710, 748172345}},
    {{-32208, -22992, 32007, 1889}, {-1072127375, -765348752, 1065436565, 62880297}},
    {{1381, -14205, 3537, -25357}, {45970191, -472850514, 117738280, -844073952}},
    {{22202, 1139, 18137, 13428}, {739051539, 37914589, 603737401, 446986040}},
    {{12993, 15140, -24275, -4061}, {432505930, 503974430, -808056757, -135180988}},
    {{8506, -15272, 18095, -19868}, {283144419, -508368395, 602339321, -661358255}},
    {{2107, -28493, -26136, -1736}, {70136996, -948463900, -870005001, -57787293}},
    {{-15567, 17923, -23006, -21038}, {-518188240, 596613852, -765814779, -700304760}},
    {{-21596, -18397, 6679, 20151}, {-718879247, -612392179, 222327954, 670778649}},
    {{27818, 11109, 9864, -13918}, {925994763, 369792071, 328348995, -463296970}},
    {{-14649, 8387, -23738, 25358}, {-487630213, 279183193, -790181310, 844107240}},
};

static const haetae_root_vec4 haeta_ct_len1_odd_v4[16]
    __attribute__((aligned(16))) = {
    {{27092, 25397, -24976, 26974}, {901827957, 845405456, -831391372, 897900019}},
    {{-737, 30144, -31458, 27827}, {-24532969, 1003421746, -1047161667, 926294351}},
    {{1732, -21646, 7052, -19002}, {57654142, -720543628, 234744233, -632531184}},
    {{6087, 5499, 5266, -30949}, {202621688, 183048573, 175292559, -1030218273}},
    {{-27091, -9244, -22250, -2327}, {-901794669, -307710676, -740649345, -77460271}},
    {{-19187, -7159, -24336, -21699}, {-638689392, -238306007, -810087301, -722307871}},
    {{11565, -20203, -1632, 24121}, {384971221, -672509605, -54325381, 802930465}},
    {{19376, 27965, 1416, -29467}, {644980751, 930888041, 47135257, -980886033}},
    {{12062, 8226, 9778, 16210}, {401515164, 273823888, 325486260, 539592174}},
    {{-12363, -31804, 12388, -15962}, {-411534735, -1058679180, 412366925, -531336847}},
    {{9096, 31587, 24371, 6437}, {302784110, 1051455768, 811252368, 214272352}},
    {{-11687, -15073, 21547, 1056}, {-389032310, -501744160, 717248154, 35151717}},
    {{-24125, 31908, 27670, 7446}, {-803063615, 1062141092, 921068196, 247859551}},
    {{-19863, -23890, 31018, 25796}, {-661191817, -795241027, 1032515118, 858687213}},
    {{-31489, -21325, 10351, -17993}, {-1048193583, -709858304, 344560061, -598943985}},
    {{-2089, -3318, 14737, -6209}, {-69537819, -110448293, 490559523, -206682777}},
};

static const haetae_root_vec4 haeta_gs_len1_even_v4[16]
    __attribute__((aligned(16))) = {
    {{6209, -14737, 3318, 2089}, {206682777, -490559523, 110448293, 69537819}},
    {{17993, -10351, 21325, 31489}, {598943985, -344560061, 709858304, 1048193583}},
    {{-25796, -31018, 23890, 19863}, {-858687213, -1032515118, 795241027, 661191817}},
    {{-7446, -27670, -31908, 24125}, {-247859551, -921068196, -1062141092, 803063615}},
    {{-1056, -21547, 15073, 11687}, {-35151717, -717248154, 501744160, 389032310}},
    {{-6437, -24371, -31587, -9096}, {-214272352, -811252368, -1051455768, -302784110}},
    {{15962, -12388, 31804, 12363}, {531336847, -412366925, 1058679180, 411534735}},
    {{-16210, -9778, -8226, -12062}, {-539592174, -325486260, -273823888, -401515164}},
    {{29467, -1416, -27965, -19376}, {980886033, -47135257, -930888041, -644980751}},
    {{-24121, 1632, 20203, -11565}, {-802930465, 54325381, 672509605, -384971221}},
    {{21699, 24336, 7159, 19187}, {722307871, 810087301, 238306007, 638689392}},
    {{2327, 22250, 9244, 27091}, {77460271, 740649345, 307710676, 901794669}},
    {{30949, -5266, -5499, -6087}, {1030218273, -175292559, -183048573, -202621688}},
    {{19002, -7052, 21646, -1732}, {632531184, -234744233, 720543628, -57654142}},
    {{-27827, 31458, -30144, 737}, {-926294351, 1047161667, -1003421746, 24532969}},
    {{-26974, 24976, -25397, -27092}, {-897900019, 831391372, -845405456, -901827957}},
};

static const haetae_root_vec4 haeta_gs_len1_odd_v4[16]
    __attribute__((aligned(16))) = {
    {{-25358, 23738, -8387, 14649}, {-844107240, 790181310, -279183193, 487630213}},
    {{13918, -9864, -11109, -27818}, {463296970, -328348995, -369792071, -925994763}},
    {{-20151, -6679, 18397, 21596}, {-670778649, -222327954, 612392179, 718879247}},
    {{21038, 23006, -17923, 15567}, {700304760, 765814779, -596613852, 518188240}},
    {{1736, 26136, 28493, -2107}, {57787293, 870005001, 948463900, -70136996}},
    {{19868, -18095, 15272, -8506}, {661358255, -602339321, 508368395, -283144419}},
    {{4061, 24275, -15140, -12993}, {135180988, 808056757, -503974430, -432505930}},
    {{-13428, -18137, -1139, -22202}, {-446986040, -603737401, -37914589, -739051539}},
    {{25357, -3537, 14205, -1381}, {844073952, -117738280, 472850514, -45970191}},
    {{-1889, -32007, 22992, 32208}, {-62880297, -1065436565, 765348752, 1072127375}},
    {{-22476, -4818, -2483, -32031}, {-748172345, -160379710, -82653138, -1066235468}},
    {{-10179, 23048, -1512, 29263}, {-338834592, 767212858, -50330868, 974095360}},
    {{-3471, -7960, 1709, -9054}, {-115541298, -264969384, 56888527, -301386030}},
    {{12748, 13548, 22086, 12622}, {424350465, 450980554, 735190176, 420156226}},
    {{-30008, 27460, 14366, -28092}, {-998894631, 914077798, 478209820, -935115568}},
    {{15997, -9780, 21713, -426}, {532501913, -325552836, 722773897, -14180522}},
};


static const haetae_root_pair haeta_scale = {3836, 127691276};
static const haetae_root_pair haeta_rinv = {-14127, -470254080};

/*
 * Constant-time argument for every function in this file:
 * loop counts and addresses depend only on public N=256 and fixed layer/block
 * numbers. MVE predicates select by public lane position or arithmetic sign;
 * there is no secret-dependent branch or address.
 */
static inline int32x4_t mulmod_n(int32x4_t value,
                                 const haetae_root_pair *root) {
  int32x4_t low = vmulq_n_s32(value, root->root);
  int32x4_t quotient = vqrdmulhq_n_s32(value, root->twisted);
  return vmlaq_n_s32(low, quotient, -HAETAE_Q);
}

static inline int32x4_t mulmod_neg_n(int32x4_t value,
                                     const haetae_root_pair *root) {
  int32x4_t low = vmulq_n_s32(value, -root->root);
  int32x4_t quotient = vqrdmulhq_n_s32(value, -root->twisted);
  return vmlaq_n_s32(low, quotient, -HAETAE_Q);
}

static inline int32x4_t mulmod_v(int32x4_t value, int32x4_t root,
                                 int32x4_t twisted) {
  int32x4_t low = vmulq_s32(value, root);
  int32x4_t quotient = vqrdmulhq_s32(value, twisted);
  return vmlaq_n_s32(low, quotient, -HAETAE_Q);
}

static inline int32x4_t load_root_lanes(const int32_t lanes[4]) {
  return vld1q_s32(lanes);
}

static inline int32x4_t rotate_lanes_2(int32x4_t value);

static inline void ct_butterfly_lanes(int32x4_t *left, int32x4_t *right,
                                      int32x4_t root, int32x4_t twisted) {
  int32x4_t product = mulmod_v(*right, root, twisted);
  int32x4_t original = *left;
  *left = vaddq_s32(original, product);
  *right = vsubq_s32(original, product);
}

static inline void ct_butterfly_v(int32x4_t *left, int32x4_t *right,
                                  const haetae_root_vec4 *root) {
  ct_butterfly_lanes(left, right, load_root_lanes(root->root),
                     load_root_lanes(root->twisted));
}

/* root is already negated in each inverse vector table. */
static inline void gs_butterfly_lanes(int32x4_t *left, int32x4_t *right,
                                      int32x4_t root, int32x4_t twisted) {
  int32x4_t difference = vsubq_s32(*left, *right);
  *left = vaddq_s32(*left, *right);
  *right = mulmod_v(difference, root, twisted);
}

static inline void gs_butterfly_v(int32x4_t *left, int32x4_t *right,
                                  const haetae_root_vec4 *root) {
  gs_butterfly_lanes(left, right, load_root_lanes(root->root),
                     load_root_lanes(root->twisted));
}

static inline int32x4_t pair_lanes(const int32_t lanes[4], unsigned pair) {
  unsigned first = 2 * pair;
  return (int32x4_t){lanes[first], lanes[first], lanes[first + 1],
                     lanes[first + 1]};
}

static inline int32x4_t alternating_lanes(const int32_t even[4],
                                           const int32_t odd[4],
                                           unsigned half) {
  unsigned first = 2 * half;
  return (int32x4_t){even[first], odd[first], even[first + 1],
                     odd[first + 1]};
}

static inline int32x4_t ct_layer_len2_v(int32x4_t value, int32x4_t root,
                                        int32x4_t twisted) {
  int32x4_t rotated = rotate_lanes_2(value);
  int32x4_t low = vaddq_s32(value, mulmod_v(rotated, root, twisted));
  int32x4_t high = vsubq_s32(rotated, mulmod_v(value, root, twisted));
  return vpselq_s32(low, high, vctp32q(2));
}

/* root/twisted are pre-negated inverse roots. */
static inline int32x4_t gs_layer_len2_v(int32x4_t value, int32x4_t root,
                                        int32x4_t twisted) {
  int32x4_t rotated = rotate_lanes_2(value);
  int32x4_t low = vaddq_s32(value, rotated);
  int32x4_t high = mulmod_v(vsubq_s32(rotated, value), root, twisted);
  return vpselq_s32(low, high, vctp32q(2));
}

static inline int32x4_t ct_layer_adjacent_v(int32x4_t value, int32x4_t root,
                                            int32x4_t twisted) {
  int32x4_t rotated = vrev64q_s32(value);
  int32x4_t low = vaddq_s32(value, mulmod_v(rotated, root, twisted));
  int32x4_t high = vsubq_s32(rotated, mulmod_v(value, root, twisted));
  return vpselq_s32(low, high, (mve_pred16_t)0x0F0F);
}

/* root/twisted are pre-negated inverse roots. */
static inline int32x4_t gs_layer_adjacent_v(int32x4_t value, int32x4_t root,
                                            int32x4_t twisted) {
  int32x4_t rotated = vrev64q_s32(value);
  int32x4_t low = vaddq_s32(value, rotated);
  int32x4_t high = mulmod_v(vsubq_s32(rotated, value), root, twisted);
  return vpselq_s32(low, high, (mve_pred16_t)0x0F0F);
}

static inline void ct_butterfly(int32x4_t *left, int32x4_t *right,
                                const haetae_root_pair *root) {
  int32x4_t product = mulmod_n(*right, root);
  int32x4_t original = *left;
  *left = vaddq_s32(original, product);
  *right = vsubq_s32(original, product);
}

static inline void gs_butterfly(int32x4_t *left, int32x4_t *right,
                                const haetae_root_pair *root) {
  int32x4_t difference = vsubq_s32(*left, *right);
  *left = vaddq_s32(*left, *right);
  *right = mulmod_neg_n(difference, root);
}

static inline int32x4_t rotate_lanes_2(int32x4_t value) {
  return (int32x4_t){value[2], value[3], value[0], value[1]};
}

static inline int32x4_t roots_2(int32_t first, int32_t second) {
  return (int32x4_t){first, first, second, second};
}

static inline int32x4_t ct_layer_len2(int32x4_t value,
                                      const haetae_root_pair *root) {
  int32x4_t rotated = rotate_lanes_2(value);
  int32x4_t low = vaddq_s32(value, mulmod_n(rotated, root));
  int32x4_t high = vsubq_s32(rotated, mulmod_n(value, root));
  return vpselq_s32(low, high, vctp32q(2));
}

static inline int32x4_t ct_layer_len1(int32x4_t value,
                                      const haetae_root_pair *first,
                                      const haetae_root_pair *second) {
  int32x4_t rotated = vrev64q_s32(value);
  int32x4_t roots = roots_2(first->root, second->root);
  int32x4_t twisted = roots_2(first->twisted, second->twisted);
  int32x4_t low = vaddq_s32(value, mulmod_v(rotated, roots, twisted));
  int32x4_t high = vsubq_s32(rotated, mulmod_v(value, roots, twisted));
  return vpselq_s32(low, high, (mve_pred16_t)0x0F0F);
}

static inline int32x4_t gs_layer_len1(int32x4_t value,
                                      const haetae_root_pair *first,
                                      const haetae_root_pair *second) {
  int32x4_t rotated = vrev64q_s32(value);
  int32x4_t roots = roots_2(-first->root, -second->root);
  int32x4_t twisted = roots_2(-first->twisted, -second->twisted);
  int32x4_t low = vaddq_s32(value, rotated);
  int32x4_t high = mulmod_v(vsubq_s32(rotated, value), roots, twisted);
  return vpselq_s32(low, high, (mve_pred16_t)0x0F0F);
}

static inline int32x4_t gs_layer_len2(int32x4_t value,
                                      const haetae_root_pair *root) {
  int32x4_t rotated = rotate_lanes_2(value);
  int32x4_t low = vaddq_s32(value, rotated);
  int32x4_t high = mulmod_neg_n(vsubq_s32(rotated, value), root);
  return vpselq_s32(low, high, vctp32q(2));
}

/*
 * A1 forward bounds, assuming |input| <= q:
 * every Barrett product is at most q/2 plus a small (< 5 here) reciprocal
 * approximation error. After the first four merged layers |a| < 3q+20;
 * after the second four |a| < 5q+40 < 323000, far below INT32_MAX.
 * The first pass loads/stores each polynomial coefficient once; the second
 * pass does the same, giving the required 4+4 two-pass layout.
 */
void haetae_ntt_mve_l44(int32_t a[HAETAE_MVE_N]) {
  for (unsigned offset = 0; offset < 16; offset += 4) {
    int32x4_t data[16];
    unsigned root_index = 1;
    for (unsigned i = 0; i < 16; i++)
      data[i] = vld1q_s32(a + 16 * i + offset);

    for (unsigned gap = 8; gap != 0; gap >>= 1) {
      for (unsigned base = 0; base < 16; base += 2 * gap) {
        const haetae_root_pair *root = &haeta_roots[root_index++];
        for (unsigned i = 0; i < gap; i++)
          ct_butterfly(&data[base + i], &data[base + gap + i], root);
      }
    }

    for (unsigned i = 0; i < 16; i++)
      vst1q_s32(a + 16 * i + offset, data[i]);
  }

  for (unsigned block = 0; block < 16; block++) {
    int32x4_t data[4];
    for (unsigned i = 0; i < 4; i++)
      data[i] = vld1q_s32(a + 16 * block + 4 * i);

    const haetae_root_pair *root = &haeta_roots[16 + block];
    ct_butterfly(&data[0], &data[2], root);
    ct_butterfly(&data[1], &data[3], root);

    ct_butterfly(&data[0], &data[1], &haeta_roots[32 + 2 * block]);
    ct_butterfly(&data[2], &data[3], &haeta_roots[33 + 2 * block]);

    for (unsigned i = 0; i < 4; i++) {
      data[i] = ct_layer_len2(data[i], &haeta_roots[64 + 4 * block + i]);
      data[i] = ct_layer_len1(data[i],
          &haeta_roots[128 + 8 * block + 2 * i],
          &haeta_roots[129 + 8 * block + 2 * i]);
      vst1q_s32(a + 16 * block + 4 * i, data[i]);
    }
  }
}

/*
 * A1 inverse bounds, assuming |input| < q:
 * an unreduced GS sum can at most double per layer. The largest value before
 * the fused final scale is below 256q = 16515328, still inside int32_t.
 * Each difference is immediately reduced. The final f=R^2/256 Montgomery
 * multiplication is fused into layer len=128 as multiplication by R/256 in
 * the standard domain, returning every coefficient to a small representative.
 */
void haetae_invntt_mve_l44(int32_t a[HAETAE_MVE_N]) {
  for (unsigned block = 0; block < 16; block++) {
    int32x4_t data[4];
    for (unsigned i = 0; i < 4; i++) {
      data[i] = vld1q_s32(a + 16 * block + 4 * i);
      data[i] = gs_layer_len1(data[i],
          &haeta_roots[255 - (8 * block + 2 * i)],
          &haeta_roots[254 - (8 * block + 2 * i)]);
      data[i] = gs_layer_len2(data[i],
          &haeta_roots[127 - (4 * block + i)]);
    }

    gs_butterfly(&data[0], &data[1],
                 &haeta_roots[63 - 2 * block]);
    gs_butterfly(&data[2], &data[3],
                 &haeta_roots[62 - 2 * block]);

    const haetae_root_pair *root = &haeta_roots[31 - block];
    gs_butterfly(&data[0], &data[2], root);
    gs_butterfly(&data[1], &data[3], root);

    for (unsigned i = 0; i < 4; i++)
      vst1q_s32(a + 16 * block + 4 * i, data[i]);
  }

  for (unsigned offset = 0; offset < 16; offset += 4) {
    int32x4_t data[16];
    int root_index = 15;
    for (unsigned i = 0; i < 16; i++)
      data[i] = vld1q_s32(a + 16 * i + offset);

    for (unsigned gap = 1; gap < 8; gap <<= 1) {
      for (unsigned base = 0; base < 16; base += 2 * gap) {
        const haetae_root_pair *root = &haeta_roots[root_index--];
        for (unsigned i = 0; i < gap; i++)
          gs_butterfly(&data[base + i], &data[base + gap + i], root);
      }
    }

    for (unsigned i = 0; i < 8; i++) {
      gs_butterfly(&data[i], &data[i + 8], &haeta_roots[1]);
      data[i] = mulmod_n(data[i], &haeta_scale);
      data[i + 8] = mulmod_n(data[i + 8], &haeta_scale);
    }

    for (unsigned i = 0; i < 16; i++)
      vst1q_s32(a + 16 * i + offset, data[i]);
  }
}

/*
 * A2 uses the same arithmetic bounds as A1, but maps 3+3+2 layers to three
 * polynomial passes. Each of the first two passes holds at most Q0-Q7 worth
 * of live coefficient vectors; the final two layers operate inside one Q
 * register. This avoids A1's 16-vector spill pressure.
 */
void haetae_ntt_mve_l332(int32_t a[HAETAE_MVE_N]) {
  for (unsigned offset = 0; offset < 32; offset += 4) {
    int32x4_t data[8];
    unsigned root_index = 1;
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * i + offset);
    for (unsigned gap = 4; gap != 0; gap >>= 1) {
      for (unsigned base = 0; base < 8; base += 2 * gap) {
        const haetae_root_pair *root = &haeta_roots[root_index++];
        for (unsigned i = 0; i < gap; i++)
          ct_butterfly(&data[base + i], &data[base + gap + i], root);
      }
    }
    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * i + offset, data[i]);
  }

  for (unsigned block = 0; block < 8; block++) {
    int32x4_t data[8];
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * block + 4 * i);

    const haetae_root_pair *root = &haeta_roots[8 + block];
    for (unsigned i = 0; i < 4; i++)
      ct_butterfly(&data[i], &data[i + 4], root);
    for (unsigned half = 0; half < 2; half++) {
      root = &haeta_roots[16 + 2 * block + half];
      for (unsigned i = 0; i < 2; i++)
        ct_butterfly(&data[4 * half + i], &data[4 * half + 2 + i], root);
    }
    for (unsigned i = 0; i < 4; i++)
      ct_butterfly(&data[2 * i], &data[2 * i + 1],
                   &haeta_roots[32 + 4 * block + i]);

    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * block + 4 * i, data[i]);
  }

  for (unsigned vector = 0; vector < 64; vector++) {
    int32x4_t data = vld1q_s32(a + 4 * vector);
    data = ct_layer_len2(data, &haeta_roots[64 + vector]);
    data = ct_layer_len1(data, &haeta_roots[128 + 2 * vector],
                         &haeta_roots[129 + 2 * vector]);
    vst1q_s32(a + 4 * vector, data);
  }
}

void haetae_invntt_mve_l332(int32_t a[HAETAE_MVE_N]) {
  for (unsigned vector = 0; vector < 64; vector++) {
    int32x4_t data = vld1q_s32(a + 4 * vector);
    data = gs_layer_len1(data, &haeta_roots[255 - 2 * vector],
                         &haeta_roots[254 - 2 * vector]);
    data = gs_layer_len2(data, &haeta_roots[127 - vector]);
    vst1q_s32(a + 4 * vector, data);
  }

  for (unsigned block = 0; block < 8; block++) {
    int32x4_t data[8];
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * block + 4 * i);

    for (unsigned i = 0; i < 4; i++)
      gs_butterfly(&data[2 * i], &data[2 * i + 1],
                   &haeta_roots[63 - (4 * block + i)]);
    for (unsigned half = 0; half < 2; half++) {
      const haetae_root_pair *root =
          &haeta_roots[31 - (2 * block + half)];
      for (unsigned i = 0; i < 2; i++)
        gs_butterfly(&data[4 * half + i], &data[4 * half + 2 + i], root);
    }
    const haetae_root_pair *root = &haeta_roots[15 - block];
    for (unsigned i = 0; i < 4; i++)
      gs_butterfly(&data[i], &data[i + 4], root);

    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * block + 4 * i, data[i]);
  }

  for (unsigned offset = 0; offset < 32; offset += 4) {
    int32x4_t data[8];
    int root_index = 7;
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * i + offset);

    for (unsigned gap = 1; gap < 4; gap <<= 1) {
      for (unsigned base = 0; base < 8; base += 2 * gap) {
        const haetae_root_pair *root = &haeta_roots[root_index--];
        for (unsigned i = 0; i < gap; i++)
          gs_butterfly(&data[base + i], &data[base + gap + i], root);
      }
    }
    for (unsigned i = 0; i < 4; i++) {
      gs_butterfly(&data[i], &data[i + 4], &haeta_roots[1]);
      data[i] = mulmod_n(data[i], &haeta_scale);
      data[i + 4] = mulmod_n(data[i + 4], &haeta_scale);
    }

    for (unsigned i = 0; i < 8; i++)
    vst1q_s32(a + 32 * i + offset, data[i]);
  }
}

static __attribute__((always_inline)) inline void
l332_forward_first3(int32_t a[HAETAE_MVE_N]) {
  for (unsigned offset = 0; offset < 32; offset += 4) {
    int32x4_t data[8];
    unsigned root_index = 1;
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * i + offset);
    for (unsigned gap = 4; gap != 0; gap >>= 1) {
      for (unsigned base = 0; base < 8; base += 2 * gap) {
        const haetae_root_pair *root = &haeta_roots[root_index++];
        for (unsigned i = 0; i < gap; i++)
          ct_butterfly(&data[base + i], &data[base + gap + i], root);
      }
    }
    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * i + offset, data[i]);
  }
}

static __attribute__((always_inline)) inline void
l332_forward_middle3(int32_t a[HAETAE_MVE_N]) {
  for (unsigned block = 0; block < 8; block++) {
    int32x4_t data[8];
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * block + 4 * i);

    const haetae_root_pair *root = &haeta_roots[8 + block];
    for (unsigned i = 0; i < 4; i++)
      ct_butterfly(&data[i], &data[i + 4], root);
    for (unsigned half = 0; half < 2; half++) {
      root = &haeta_roots[16 + 2 * block + half];
      for (unsigned i = 0; i < 2; i++)
        ct_butterfly(&data[4 * half + i], &data[4 * half + 2 + i], root);
    }
    for (unsigned i = 0; i < 4; i++)
      ct_butterfly(&data[2 * i], &data[2 * i + 1],
                   &haeta_roots[32 + 4 * block + i]);

    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * block + 4 * i, data[i]);
  }
}

static __attribute__((always_inline)) inline void
l332_forward_middle2(int32_t a[HAETAE_MVE_N]) {
  for (unsigned block = 0; block < 8; block++) {
    int32x4_t data[8];
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * block + 4 * i);

    const haetae_root_pair *root = &haeta_roots[8 + block];
    for (unsigned i = 0; i < 4; i++)
      ct_butterfly(&data[i], &data[i + 4], root);
    for (unsigned half = 0; half < 2; half++) {
      root = &haeta_roots[16 + 2 * block + half];
      for (unsigned i = 0; i < 2; i++)
        ct_butterfly(&data[4 * half + i], &data[4 * half + 2 + i], root);
    }

    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * block + 4 * i, data[i]);
  }
}

static __attribute__((always_inline)) inline void
l332_inverse_middle3(int32_t a[HAETAE_MVE_N]) {
  for (unsigned block = 0; block < 8; block++) {
    int32x4_t data[8];
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * block + 4 * i);

    for (unsigned i = 0; i < 4; i++)
      gs_butterfly(&data[2 * i], &data[2 * i + 1],
                   &haeta_roots[63 - (4 * block + i)]);
    for (unsigned half = 0; half < 2; half++) {
      const haetae_root_pair *root =
          &haeta_roots[31 - (2 * block + half)];
      for (unsigned i = 0; i < 2; i++)
        gs_butterfly(&data[4 * half + i], &data[4 * half + 2 + i], root);
    }
    const haetae_root_pair *root = &haeta_roots[15 - block];
    for (unsigned i = 0; i < 4; i++)
      gs_butterfly(&data[i], &data[i + 4], root);

    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * block + 4 * i, data[i]);
  }
}

static __attribute__((always_inline)) inline void
l332_inverse_middle2(int32_t a[HAETAE_MVE_N]) {
  for (unsigned block = 0; block < 8; block++) {
    int32x4_t data[8];
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * block + 4 * i);

    for (unsigned half = 0; half < 2; half++) {
      const haetae_root_pair *root =
          &haeta_roots[31 - (2 * block + half)];
      for (unsigned i = 0; i < 2; i++)
        gs_butterfly(&data[4 * half + i], &data[4 * half + 2 + i], root);
    }
    const haetae_root_pair *root = &haeta_roots[15 - block];
    for (unsigned i = 0; i < 4; i++)
      gs_butterfly(&data[i], &data[i + 4], root);

    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * block + 4 * i, data[i]);
  }
}

static __attribute__((always_inline)) inline void
l332_inverse_last3(int32_t a[HAETAE_MVE_N]) {
  for (unsigned offset = 0; offset < 32; offset += 4) {
    int32x4_t data[8];
    int root_index = 7;
    for (unsigned i = 0; i < 8; i++)
      data[i] = vld1q_s32(a + 32 * i + offset);

    for (unsigned gap = 1; gap < 4; gap <<= 1) {
      for (unsigned base = 0; base < 8; base += 2 * gap) {
        const haetae_root_pair *root = &haeta_roots[root_index--];
        for (unsigned i = 0; i < gap; i++)
          gs_butterfly(&data[base + i], &data[base + gap + i], root);
      }
    }
    for (unsigned i = 0; i < 4; i++) {
      gs_butterfly(&data[i], &data[i + 4], &haeta_roots[1]);
      data[i] = mulmod_n(data[i], &haeta_scale);
      data[i + 4] = mulmod_n(data[i + 4], &haeta_scale);
    }

    for (unsigned i = 0; i < 8; i++)
      vst1q_s32(a + 32 * i + offset, data[i]);
  }
}

void haetae_ntt_mve_l332_vld2(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  l332_forward_first3(a);
  l332_forward_middle3(a);

  for (unsigned chunk = 0; chunk < 16; chunk++) {
    int32x4x2_t low = vld2q_s32(a + 16 * chunk);
    int32x4x2_t high = vld2q_s32(a + 16 * chunk + 8);
    const haetae_root_vec4 *len2 = &haeta_ct_len2_v4[chunk];
    const haetae_root_vec4 *even = &haeta_ct_len1_even_v4[chunk];
    const haetae_root_vec4 *odd = &haeta_ct_len1_odd_v4[chunk];

    int32x4_t roots = pair_lanes(len2->root, 0);
    int32x4_t twisted = pair_lanes(len2->twisted, 0);
    low.val[0] = ct_layer_adjacent_v(low.val[0], roots, twisted);
    low.val[1] = ct_layer_adjacent_v(low.val[1], roots, twisted);

    roots = pair_lanes(len2->root, 1);
    twisted = pair_lanes(len2->twisted, 1);
    high.val[0] = ct_layer_adjacent_v(high.val[0], roots, twisted);
    high.val[1] = ct_layer_adjacent_v(high.val[1], roots, twisted);

    roots = alternating_lanes(even->root, odd->root, 0);
    twisted = alternating_lanes(even->twisted, odd->twisted, 0);
    ct_butterfly_lanes(&low.val[0], &low.val[1], roots, twisted);
    roots = alternating_lanes(even->root, odd->root, 1);
    twisted = alternating_lanes(even->twisted, odd->twisted, 1);
    ct_butterfly_lanes(&high.val[0], &high.val[1], roots, twisted);

    vst2q_s32(a + 16 * chunk, low);
    vst2q_s32(a + 16 * chunk + 8, high);
  }
}

void haetae_invntt_mve_l332_vld2(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  for (unsigned chunk = 0; chunk < 16; chunk++) {
    int32x4x2_t low = vld2q_s32(a + 16 * chunk);
    int32x4x2_t high = vld2q_s32(a + 16 * chunk + 8);
    const haetae_root_vec4 *len2 = &haeta_gs_len2_v4[chunk];
    const haetae_root_vec4 *even = &haeta_gs_len1_even_v4[chunk];
    const haetae_root_vec4 *odd = &haeta_gs_len1_odd_v4[chunk];

    int32x4_t roots = alternating_lanes(even->root, odd->root, 0);
    int32x4_t twisted = alternating_lanes(even->twisted, odd->twisted, 0);
    gs_butterfly_lanes(&low.val[0], &low.val[1], roots, twisted);
    roots = alternating_lanes(even->root, odd->root, 1);
    twisted = alternating_lanes(even->twisted, odd->twisted, 1);
    gs_butterfly_lanes(&high.val[0], &high.val[1], roots, twisted);

    roots = pair_lanes(len2->root, 0);
    twisted = pair_lanes(len2->twisted, 0);
    low.val[0] = gs_layer_adjacent_v(low.val[0], roots, twisted);
    low.val[1] = gs_layer_adjacent_v(low.val[1], roots, twisted);
    roots = pair_lanes(len2->root, 1);
    twisted = pair_lanes(len2->twisted, 1);
    high.val[0] = gs_layer_adjacent_v(high.val[0], roots, twisted);
    high.val[1] = gs_layer_adjacent_v(high.val[1], roots, twisted);

    vst2q_s32(a + 16 * chunk, low);
    vst2q_s32(a + 16 * chunk + 8, high);
  }

  l332_inverse_middle3(a);
  l332_inverse_last3(a);
}

static __attribute__((always_inline)) inline void
l332_forward_lower2_vld4_chunk(int32_t a[HAETAE_MVE_N], unsigned chunk) {
  int32x4x4_t data = vld4q_s32(a + 16 * chunk);
  ct_butterfly_v(&data.val[0], &data.val[2], &haeta_ct_len2_v4[chunk]);
  ct_butterfly_v(&data.val[1], &data.val[3], &haeta_ct_len2_v4[chunk]);
  ct_butterfly_v(&data.val[0], &data.val[1],
                 &haeta_ct_len1_even_v4[chunk]);
  ct_butterfly_v(&data.val[2], &data.val[3],
                 &haeta_ct_len1_odd_v4[chunk]);
  vst4q_s32(a + 16 * chunk, data);
}

static __attribute__((always_inline)) inline void
l332_inverse_lower2_vld4_chunk(int32_t a[HAETAE_MVE_N], unsigned chunk) {
  int32x4x4_t data = vld4q_s32(a + 16 * chunk);
  gs_butterfly_v(&data.val[0], &data.val[1],
                 &haeta_gs_len1_even_v4[chunk]);
  gs_butterfly_v(&data.val[2], &data.val[3],
                 &haeta_gs_len1_odd_v4[chunk]);
  gs_butterfly_v(&data.val[0], &data.val[2], &haeta_gs_len2_v4[chunk]);
  gs_butterfly_v(&data.val[1], &data.val[3], &haeta_gs_len2_v4[chunk]);
  vst4q_s32(a + 16 * chunk, data);
}

void haetae_ntt_mve_l332_vld4(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  l332_forward_first3(a);
  l332_forward_middle3(a);
  for (unsigned chunk = 0; chunk < 16; chunk++)
    l332_forward_lower2_vld4_chunk(a, chunk);
}

void haetae_invntt_mve_l332_vld4(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  for (unsigned chunk = 0; chunk < 16; chunk++)
    l332_inverse_lower2_vld4_chunk(a, chunk);
  l332_inverse_middle3(a);
  l332_inverse_last3(a);
}

void haetae_ntt_mve_l332_vld4_u2(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  l332_forward_first3(a);
  l332_forward_middle3(a);
  for (unsigned chunk = 0; chunk < 16; chunk += 2) {
    l332_forward_lower2_vld4_chunk(a, chunk);
    l332_forward_lower2_vld4_chunk(a, chunk + 1);
  }
}

void haetae_invntt_mve_l332_vld4_u2(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  for (unsigned chunk = 0; chunk < 16; chunk += 2) {
    l332_inverse_lower2_vld4_chunk(a, chunk);
    l332_inverse_lower2_vld4_chunk(a, chunk + 1);
  }
  l332_inverse_middle3(a);
  l332_inverse_last3(a);
}

void haetae_ntt_mve_l332_vld4_m3(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  l332_forward_first3(a);
  l332_forward_middle2(a);

  for (unsigned chunk = 0; chunk < 16; chunk++) {
    int32x4x4_t data = vld4q_s32(a + 16 * chunk);
    const haetae_root_vec4 *len4 = &haeta_ct_len4_v4[chunk >> 1];
    unsigned pair = chunk & 1u;
    int32x4_t roots = pair_lanes(len4->root, pair);
    int32x4_t twisted = pair_lanes(len4->twisted, pair);

    for (unsigned lane = 0; lane < 4; lane++)
      data.val[lane] = ct_layer_adjacent_v(data.val[lane], roots, twisted);
    ct_butterfly_v(&data.val[0], &data.val[2],
                   &haeta_ct_len2_v4[chunk]);
    ct_butterfly_v(&data.val[1], &data.val[3],
                   &haeta_ct_len2_v4[chunk]);
    ct_butterfly_v(&data.val[0], &data.val[1],
                   &haeta_ct_len1_even_v4[chunk]);
    ct_butterfly_v(&data.val[2], &data.val[3],
                   &haeta_ct_len1_odd_v4[chunk]);
    vst4q_s32(a + 16 * chunk, data);
  }
}

void haetae_invntt_mve_l332_vld4_m3(int32_t a[HAETAE_MVE_N]) {
  /* CT: fixed public layer/chunk counts, fixed addresses, no secret control. */
  for (unsigned chunk = 0; chunk < 16; chunk++) {
    int32x4x4_t data = vld4q_s32(a + 16 * chunk);
    const haetae_root_vec4 *len4 = &haeta_gs_len4_v4[chunk >> 1];
    unsigned pair = chunk & 1u;

    gs_butterfly_v(&data.val[0], &data.val[1],
                   &haeta_gs_len1_even_v4[chunk]);
    gs_butterfly_v(&data.val[2], &data.val[3],
                   &haeta_gs_len1_odd_v4[chunk]);
    gs_butterfly_v(&data.val[0], &data.val[2],
                   &haeta_gs_len2_v4[chunk]);
    gs_butterfly_v(&data.val[1], &data.val[3],
                   &haeta_gs_len2_v4[chunk]);

    int32x4_t roots = pair_lanes(len4->root, pair);
    int32x4_t twisted = pair_lanes(len4->twisted, pair);
    for (unsigned lane = 0; lane < 4; lane++)
      data.val[lane] = gs_layer_adjacent_v(data.val[lane], roots, twisted);
    vst4q_s32(a + 16 * chunk, data);
  }

  l332_inverse_middle2(a);
  l332_inverse_last3(a);
}

static inline int32x4_t centered_mod(int32x4_t value, int32_t modulus,
                                      int32_t reciprocal) {
  int32x4_t quotient = vqrdmulhq_n_s32(value, reciprocal);
  int32x4_t reduced = vmlaq_n_s32(value, quotient, -modulus);
  const int32_t half = modulus / 2;

  int32x4_t minus = vsubq_n_s32(reduced, modulus);
  reduced = vpselq_s32(minus, reduced, vcmpgtq_n_s32(reduced, half));
  int32x4_t plus = vaddq_n_s32(reduced, modulus);
  return vpselq_s32(plus, reduced, vcmpltq_n_s32(reduced, -half));
}

static inline int32x4_t barrett_once_q(int32x4_t value) {
  int32x4_t quotient = vqrdmulhq_n_s32(value, 33288);
  return vmlaq_n_s32(value, quotient, -HAETAE_Q);
}

static inline int32x4_t freeze_mod(int32x4_t value, int32_t modulus,
                                    int32_t reciprocal) {
  int32x4_t reduced = centered_mod(value, modulus, reciprocal);
  return vpselq_s32(vaddq_n_s32(reduced, modulus), reduced,
                    vcmpltq_n_s32(reduced, 0));
}

/*
 * Corrected centering bounds inputs by 32256; the selected one-pass form uses
 * the proven Bt=32257 (epsilon=1), so either lane product is at most
 * 32257^2=1040514049 < INT32_MAX. Product reduction followed by
 * R^-1=-14127 implements the clean Montgomery result modulo q.
 * CT: the 64 iterations and affine addresses depend only on public N=256;
 * coefficient values affect arithmetic only, never control or memory access.
 */
void haetae_pointwise_mve(int32_t c[HAETAE_MVE_N],
                          const int32_t a[HAETAE_MVE_N],
                          const int32_t b[HAETAE_MVE_N]) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4) {
#if defined(HAETAE_POINTWISE_BARRETT_TIGHT)
    int32x4_t left = barrett_once_q(vld1q_s32(a + i));
    int32x4_t right = barrett_once_q(vld1q_s32(b + i));
    int32x4_t product = barrett_once_q(vmulq_s32(left, right));
#else
    int32x4_t left = centered_mod(vld1q_s32(a + i), HAETAE_Q, 33288);
    int32x4_t right = centered_mod(vld1q_s32(b + i), HAETAE_Q, 33288);
    int32x4_t product = vmulq_s32(left, right);
    product = centered_mod(product, HAETAE_Q, 33288);
#endif
    vst1q_s32(c + i, mulmod_n(product, &haeta_rinv));
  }
}

/*
 * Fused NTT-domain inner product, with one compile-time ACC_VARIANT.
 *
 * Exact no-overflow proof required by Phase 3.7-2 (q=64513): corrected
 * centering gives Bc=32256 (epsilon=0), while one Barrett pass has
 * Bt=32257 (epsilon=1) because 33288*q-2^31=25096 and its quotient bias is
 * <25096/64513<1/2 for every int32 input. Thus Pc=Bc^2=1040449536 and
 * Pt=Bt^2=1040514049. Two raw products are at most 2080899072 or
 * 2081028098, below INT32_MAX by 66584575 or 66455549. Three products would
 * be 3121348608 or 3121542147 and are forbidden; dmax=floor(INT32_MAX/P)=2
 * for both T choices. At most ceil(7/2)=4 reduced groups sum to 4*Bt=129028.
 * The final low R^-1 product is at most 129028*14127=1822778556, leaving
 * 324705091 below INT32_MAX. No signed 32-bit arithmetic in a qualified
 * variant can overflow.
 *
 * CT argument for every compiled variant: coefficient values affect only MVE
 * arithmetic. The i/term loops, d2 grouping, and flat addresses depend only
 * on public N=256 and terms<=7; there is no secret branch, address, or trip
 * count. The d3 candidate is intentionally absent because the proof rejects
 * it before implementation.
 */
void haetae_pointwise_acc_mve(int32_t c[HAETAE_MVE_N], const int32_t *a,
                              const int32_t *b, unsigned terms) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4) {
    int32x4_t accumulated = vdupq_n_s32(0);
#if defined(HAETAE_ACC_PERTERM_D1_CENTERED)
    for (unsigned term = 0; term < terms; term++) {
      const unsigned offset = term * HAETAE_MVE_N + i;
      int32x4_t left = centered_mod(vld1q_s32(a + offset), HAETAE_Q, 33288);
      int32x4_t right = centered_mod(vld1q_s32(b + offset), HAETAE_Q, 33288);
      int32x4_t product = centered_mod(vmulq_s32(left, right), HAETAE_Q, 33288);
      accumulated = vaddq_s32(accumulated, mulmod_n(product, &haeta_rinv));
    }
#elif defined(HAETAE_ACC_AFTERSUM_D2_CENTERED) || \
    defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT) || \
    defined(HAETAE_ACC_AFTERSUM_DMAX_TIGHT_PRECENTER)
    unsigned term = 0;
    for (; term + 1 < terms; term += 2) {
      unsigned offset0 = term * HAETAE_MVE_N + i;
      unsigned offset1 = offset0 + HAETAE_MVE_N;
#if defined(HAETAE_ACC_AFTERSUM_D2_CENTERED)
      int32x4_t left0 =
          centered_mod(vld1q_s32(a + offset0), HAETAE_Q, 33288);
      int32x4_t right0 =
          centered_mod(vld1q_s32(b + offset0), HAETAE_Q, 33288);
      int32x4_t left1 =
          centered_mod(vld1q_s32(a + offset1), HAETAE_Q, 33288);
      int32x4_t right1 =
          centered_mod(vld1q_s32(b + offset1), HAETAE_Q, 33288);
      int32x4_t group = centered_mod(
          vaddq_s32(vmulq_s32(left0, right0), vmulq_s32(left1, right1)),
          HAETAE_Q, 33288);
#else
      int32x4_t left0 = barrett_once_q(vld1q_s32(a + offset0));
      int32x4_t right0 = barrett_once_q(vld1q_s32(b + offset0));
      int32x4_t left1 = barrett_once_q(vld1q_s32(a + offset1));
      int32x4_t right1 = barrett_once_q(vld1q_s32(b + offset1));
      int32x4_t group = barrett_once_q(
          vaddq_s32(vmulq_s32(left0, right0), vmulq_s32(left1, right1)));
#endif
      accumulated = vaddq_s32(accumulated, group);
    }
    if (term < terms) {
      unsigned offset = term * HAETAE_MVE_N + i;
#if defined(HAETAE_ACC_AFTERSUM_D2_CENTERED)
      int32x4_t left =
          centered_mod(vld1q_s32(a + offset), HAETAE_Q, 33288);
      int32x4_t right =
          centered_mod(vld1q_s32(b + offset), HAETAE_Q, 33288);
      accumulated = vaddq_s32(
          accumulated,
          centered_mod(vmulq_s32(left, right), HAETAE_Q, 33288));
#else
      int32x4_t left = barrett_once_q(vld1q_s32(a + offset));
      int32x4_t right = barrett_once_q(vld1q_s32(b + offset));
      accumulated = vaddq_s32(
          accumulated, barrett_once_q(vmulq_s32(left, right)));
#endif
    }
    accumulated = mulmod_n(accumulated, &haeta_rinv);
#else
#error "A supported HAETAE_ACC_VARIANT must be selected"
#endif
    vst1q_s32(c + i, accumulated);
  }
}

/*
 * Fixed public pass producing the Bt=32257, epsilon=1 representation proven
 * above. No coefficient controls an address, branch, or loop iteration.
 */
void haetae_barrett_tight_mve(int32_t dst[HAETAE_MVE_N],
                              const int32_t src[HAETAE_MVE_N]) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4)
    vst1q_s32(dst + i, barrett_once_q(vld1q_s32(src + i)));
}

/*
 * Round-1 row kernel. The right stream was produced by the fixed pass above,
 * so |right|<=32257; left uses the same one-pass bound. The exact dmax=2 and
 * final-R^-1 bounds in haetae_pointwise_acc_mve therefore apply unchanged.
 * CT: N, terms, grouping, and both affine streams are public and fixed; there
 * is no secret-dependent branch, address, or iteration count.
 */
void haetae_pointwise_acc_precentered_mve(int32_t c[HAETAE_MVE_N],
                                          const int32_t *a, const int32_t *b,
                                          unsigned terms) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4) {
    int32x4_t accumulated = vdupq_n_s32(0);
    unsigned term = 0;
    for (; term + 1 < terms; term += 2) {
      unsigned offset0 = term * HAETAE_MVE_N + i;
      unsigned offset1 = offset0 + HAETAE_MVE_N;
      int32x4_t left0 = barrett_once_q(vld1q_s32(a + offset0));
      int32x4_t right0 = vld1q_s32(b + offset0);
      int32x4_t left1 = barrett_once_q(vld1q_s32(a + offset1));
      int32x4_t right1 = vld1q_s32(b + offset1);
      int32x4_t group = barrett_once_q(
          vaddq_s32(vmulq_s32(left0, right0), vmulq_s32(left1, right1)));
      accumulated = vaddq_s32(accumulated, group);
    }
    if (term < terms) {
      unsigned offset = term * HAETAE_MVE_N + i;
      int32x4_t left = barrett_once_q(vld1q_s32(a + offset));
      int32x4_t right = vld1q_s32(b + offset);
      accumulated = vaddq_s32(
          accumulated, barrett_once_q(vmulq_s32(left, right)));
    }
    vst1q_s32(c + i, mulmod_n(accumulated, &haeta_rinv));
  }
}

/* Compact-A variant: public coefficients are stored losslessly as uint16_t
 * because q < 2^16, then zero-extended by MVE before the identical Barrett
 * arithmetic. N, terms, and both affine streams remain fixed/public. */
void haetae_pointwise_acc_u16_precentered_mve(
    int32_t c[HAETAE_MVE_N], const uint16_t *a, const int32_t *b,
    unsigned terms) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4) {
    int32x4_t accumulated = vdupq_n_s32(0);
    unsigned term = 0;
    for (; term + 1 < terms; term += 2) {
      unsigned offset0 = term * HAETAE_MVE_N + i;
      unsigned offset1 = offset0 + HAETAE_MVE_N;
      int32x4_t left0 = barrett_once_q(
          vreinterpretq_s32_u32(vldrhq_u32(a + offset0)));
      int32x4_t right0 = vld1q_s32(b + offset0);
      int32x4_t left1 = barrett_once_q(
          vreinterpretq_s32_u32(vldrhq_u32(a + offset1)));
      int32x4_t right1 = vld1q_s32(b + offset1);
      int32x4_t group = barrett_once_q(
          vaddq_s32(vmulq_s32(left0, right0), vmulq_s32(left1, right1)));
      accumulated = vaddq_s32(accumulated, group);
    }
    if (term < terms) {
      unsigned offset = term * HAETAE_MVE_N + i;
      int32x4_t left = barrett_once_q(
          vreinterpretq_s32_u32(vldrhq_u32(a + offset)));
      int32x4_t right = vld1q_s32(b + offset);
      accumulated = vaddq_s32(
          accumulated, barrett_once_q(vmulq_s32(left, right)));
    }
    vst1q_s32(c + i, mulmod_n(accumulated, &haeta_rinv));
  }
}

void haetae_reduce2q_mve(int32_t a[HAETAE_MVE_N]) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4) {
    int32x4_t value = vld1q_s32(a + i);
    vst1q_s32(a + i, centered_mod(value, HAETAE_DQ, 16644));
  }
}

void haetae_freeze2q_mve(int32_t a[HAETAE_MVE_N]) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4) {
    int32x4_t value = vld1q_s32(a + i);
    vst1q_s32(a + i, freeze_mod(value, HAETAE_DQ, 16644));
  }
}

void haetae_freeze_mve(int32_t a[HAETAE_MVE_N]) {
  for (unsigned i = 0; i < HAETAE_MVE_N; i += 4) {
    int32x4_t value = vld1q_s32(a + i);
    vst1q_s32(a + i, freeze_mod(value, HAETAE_Q, 33288));
  }
}
