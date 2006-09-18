/*
 *  File:       68K_Stub.r
 *  Summary:   Mac resources
 *  Written by: Jesse Jones
 *
 *  Change History (most recent first):
 *
 *     <1> 3/30/99 JDJ Created
 */

#include <Types.r>


// ============================================================================
// Resources
// These resources create a tiny 68K app that pops up an
// alert telling the user that the app will not run and
// exits when the user dismisses the alert. This
// file should only be added to PPC builds.
// ============================================================================
resource 'ALRT' (30000) {
	{84, 122, 174, 486},
	30000,
	{
	OK, visible, silent,
	OK, visible, silent,
	OK, visible, silent,
	OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (30000) {
	{
	{60, 280, 80, 338},
	Button {
	enabled,
	"Quit"
	},

	{10, 60, 46, 360},
	StaticText {
	disabled,
	"This program requires a PowerPC processor and will"
	"not run on your Macintosh."
	}
	}
};

data 'CODE' (1, "Sources", locked, protected, preload) {
	$"0000 0001 9DCE 598F 2F3C 434F 4445 4267"            /* ....ù'Yè/<CODEBg */
	$"A9A0 2017 6700 011E 2040 2050 2E18 2C10"            /* ©Ý .g... @P..,. */
	$"A9A3 7000 204D 91C6 6002 10C0 B1CD 6DFA"            /* ©£p.Më`..¿±ÕmT */
	$"41ED 0028 43F5 7800 6002 10C0 B1C9 6DFA"            /* AÌ.(Czx.`..¿±SmT */
	$"598F 2F3C 4441 5441 4267 A9A0 2057 2008"            /* Yè/<DATABg©Ý W . */
	$"6700 00E2 2F0D 2050 4868 0004 4EBA 00DC"            /* g..'/¬PHh..N?.Ð */
	$"508F 43FA FF9C 2B49 FF12 224D 4EBA 0244"            /* PèCTú+I."MN?.D */
	$"226D FF12 4EBA 023C A9A3 4EBA 02C2 422D"            /* "m.N?.<©£N?.¬B- */
	$"FF17 303C A89F A746 2F08 303C A198 A346"            /* .0<®üßF/.0<°ò£F */
	$"B1DF 670A 1B7C 0001 FF17 7001 A198 303C"            /* ±þg..|...p.°ò0< */
	$"A9F0 A746 2B48 FF0E 303C A9F1 A746 2B48"            /* ©.ßF+H.0<©ÒßF+H */
	$"FF0A 303C A9F4 A746 2B48 FF06 303C A9F0"            /* .0<©ÙßF+H.0<©. */
	$"41FA 0284 A647 303C A9F1 41FA 0338 A647"            /* AT.Ñ¶G0<©ÒAT.8¶G */
	$"303C A9F4 41FA 003C A647 4EB9 0000 0492"            /* 0<©ÙAT.<¶GN¼...í */
	$"4EB9 0000 04B8 487A 004E 4A2D FFFF 6702"            /* N¼...½Hz.NJ-g. */
	$"4267 4267 4EB9 0000 04E4 5C8F 4A2D FFFF"            /* BgBgN¼..."\èJ- */
	$"6702 548F 202D FF2C 6704 2040 4E90 4EBA"            /* g.Tè -,g.@NêN? */
	$"0384 2A78 0904 303C A9F4 206D FF06 A647"            /* .Ñ*x.0<©Ùm.¶G */
	$"303C A9F0 206D FF0E A647 303C A9F1 206D"            /* 0<©.m.¶G0<©Ò m */
	$"FF0A A647 A9F4 0000 0000 48E7 1C30 594F"            /* .¶G©Ù....HÁ.0YO */
	$"266F 001C 7800 6000 00E4 1E9B 1F5B 0001"            /* &o..x.`..".õ.[.. */
	$"1F5B 0002 1F5B 0003 246F 0020 D5D7 161B"            /* .[...[..$o. '×.. */
	$"4883 3003 0240 0080 670E 0243 007F 14DB"            /* HÉ0..@.Äg..C...¤ */
	$"5343 4A43 6CF8 60E6 3003 0240 0040 670E"            /* SCJCl¯`Ê0..@.@g. */
	$"3003 0240 003F 5240 48C0 D5C0 60D0 3003"            /* 0..@.?R@H¿'¿`-0. */
	$"0240 0020 670A 0243 001F 5243 1A1B 600E"            /* .@.g..C..RC..`. */
	$"3003 0240 0010 6710 0243 000F 7AFF 14C5"            /* 0..@..g..C..z.? */
	$"5343 4A43 6CF8 60A6 3003 0C40 0004 6264"            /* SCJCl¯`¶0..@..bd */
	$"D040 303B 0006 4EFB 0002 0060 000A 001C"            /* -@0;..Ns...`.... */
	$"002C 0042 588A 14FC FFFF 14FC FFFF 14DB"            /* .,.BXä.¸.¸.¤ */
	$"14DB 6000 FF7A 588A 14FC FFFF 14DB 14DB"            /* .¤`.zXä.¸.¤.¤ */
	$"14DB 6000 FF6A 14FC FFA9 14FC FFF0 548A"            /* .¤`.j.¸©.¸.Tä */
	$"14DB 14DB 528A 14DB 6000 FF54 14FC FFA9"            /* .¤.¤Rä.¤`.T.¸© */
	$"14FC FFF0 528A 14DB 14DB 14DB 528A 14DB"            /* .¸.Rä.¤.¤.¤Rä.¤ */
	$"6000 FF3C 3F3C 000F A9C9 5244 0C44 0003"            /* `.<?<..©SRD.D.. */
	$"6D00 FF18 204B 584F 4CDF 0C38 4E75 2F05"            /* m..KXOLþ.8Nu/. */
	$"594F 226F 000C 1E99 1F59 0001 1F59 0002"            /* YO"o...ô.Y...Y.. */
	$"1F59 0003 2A17 7400 604C 1219 1001 0240"            /* .Y..*.t.`L.....@ */
	$"0080 670C D201 1001 4880 48C0 D480 6028"            /* .Äg."...HÄH¿'Ä`( */
	$"1E81 1F59 0001 1001 0240 0040 670C 3017"            /* .Å.Y.....@.@g.0. */
	$"E548 E240 48C0 D480 600E 1F59 0002 1F59"            /* ÂH'@H¿'Ä`..Y...Y */
	$"0003 2417 E58A E282 206F 0010 202F 0014"            /* ..$.Âä'Ço.. /.. */
	$"D1B0 2800 5385 4A85 6EB0 2049 584F 2A1F"            /* -f(.SÖJÖnfIXO*. */
	$"4E75 2F0A 2449 2F0D 2F0A 2F08 4EBA FF80"            /* Nu/.$I/¬/./.N?Ä */
	$"2F2D FF12 2F0A 2F08 4EBA FF74 2F0A 2F0A"            /* /-././.N?t/./. */
	$"2F08 4EBA FF6A 4FEF 0024 245F 4E75 2F0A"            /* /.N?jOÔ.$$_Nu/. */
	$"2449 2F0D 4497 2F0A 2F08 4EBA FF52 2F2D"            /* $I/¬Dó/./.N?R/- */
	$"FF12 4497 2F0A 2F08 4EBA FF44 2F0A 4497"            /* .Dó/./.N?D/.Dó */
	$"2F0A 2F08 4EBA FF38 4FEF 0024 245F 4E75"            /* /./.N?8OÔ.$$_Nu */
	$"BBFA 0028 6602 4E75 48E7 0084 2A7A 001C"            /* ªT.(f.NuHÁ.Ñ*z.. */
	$"206D FF0E 4A6F 000C 6604 206D FF0A 2F48"            /*  m.Jo..f.m./H */
	$"000A 4CDF 2100 544F 4E75 0000 0000 41FA"            /* ..Lþ!.TONu....AT */
	$"FFFA 208D 4E75 3F3C 0001 4EBA FFC4 544F"            /* TçNu?<..N?YTO */
	$"48E7 E0E0 55AF 0018 246F 0018 202D FF18"            /* HÁýýUØ..$o.. -. */
	$"670A 2040 3F2A 0006 4E90 548F 50F8 0A5E"            /* g.@?*..NêTèP¯.^ */
	$"598F 2F3C 434F 4445 3F2A 0006 A9A0 2017"            /* Yè/<CODE?*..©Ý . */
	$"6616 202D FF24 6604 700F A9C9 2040 3F2A"            /* f.-$f.p.©S @?* */
	$"0006 4E90 548F 60DA 4A38 0BB2 6704 2040"            /* ..NêTè`ZJ8.¾g. @ */
	$"A064 2057 A029 205F 2050 2008 A055 2040"            /* Ýd WÝ) _ P.ÝU @ */
	$"2F08 2248 D1E8 0008 4EBA FEF8 205F 224D"            /* /."H-Ë..N?o¯ _"M */
	$"D3E8 0004 3028 0002 2208 600C 337C 4EF9"            /* "Ë..0(..".`.3|N~ */
	$"0000 D3A9 0002 5089 51C8 FFF2 4A2D FF17"            /* .."©..PâQ»ÚJ-. */
	$"6704 7001 A198 202D FF1C 670A 2040 3F2A"            /* g.p.°ò-.g. @?* */
	$"0006 4E90 548F 4CDF 0707 4A38 012D 6702"            /* ..NêTèLþ..J8.-g. */
	$"A9FF 4E75 3F3C 0000 4EBA FF06 544F 2F0A"            /* ©Nu?<..N?.TO/. */
	$"246F 0008 0C6A 4EF9 0000 6670 0C6A 0002"            /* $o...jN~..fp.j.. */
	$"0006 6D68 598F 2F3C 434F 4445 3F2A 0006"            /* ..mhYè/<CODE?*.. */
	$"A9A0 2017 6604 588F 6052 2040 2050 2248"            /* ©Ý .f.Xè`R@ P"H */
	$"D1E8 0008 4EBA FE98 2057 2050 224D D3E8"            /* -Ë..N?oò WP"M"Ë */
	$"0004 3028 0002 2208 600C 337C A9F0 0000"            /* ..0(..".`.3|©... */
	$"93A9 0002 5089 51C8 FFF2 2057 A02A 205F"            /* ì©..PâQ»ÚWÝ* _ */
	$"A049 4A2D FF17 6704 7001 A198 202D FF20"            /* ÝIJ-.g.p.°ò -  */
	$"670A 2040 3F2A 0006 4E90 548F 245F 2E9F"            /* g.@?*..NêTè$_.ü */
	$"4E75 4E75 2F0A 6014 2B52 FF28 3F3C FFFF"            /* NuNu/.`.+R(?< */
	$"2F2A 0008 206A 0004 4E90 5C4F 246D FF28"            /* /*.. j..Nê\O$m( */
	$"200A 66E4 245F 4E75 4E75 4E56 0000 486D"            /* .f"$_NuNuNV..Hm */
	$"FFFA A86E A8FE A912 A930 A9CC 42A7 A97B"            /* T®n®o©.©0©ÃBß©{ */
	$"A850 4E5E 4E75 8B49 6E69 7454 6F6F 6C62"            /* ®PN^NuãInitToolb */
	$"6F78 0000 4E56 0000 4EBA FFD0 554F 3F3C"            /* ox..NV..N?-UO?< */
	$"7530 42A7 A986 301F 4E5E 4E75 846D 6169"            /* u0Bß©Ü0.N^NuÑmai */
	$"6E00 0000"                                          /* n... */
};

data 'CODE' (0, purgeable, protected) {
	$"0000 0030 0000 0100 0000 0008 0000 0020"            /* ...0...........  */
	$"0000 3F3C 0001 A9F0"                                /* ..?<..©. */
};

data 'DATA' (0, purgeable, protected) {
	$"0000 0020 FFFF FFFF 4000 0000 0028 0000"            /* ... @....(.. */
	$"0000 2800 0000 0000 0000 0000 0000 0000"            /* ..(............. */
	$"0000 0000 0000 0003 406E 838A 0000 0000"            /* ........@nÉä.... */
};

