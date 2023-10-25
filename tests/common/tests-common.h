// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_TESTS_COMMON_H
#define JEEFS_TESTS_COMMON_H

#ifndef TEST_DIR
#define TEST_DIR "/tmp"
#endif

#ifndef TEST_EEPROM_PATH
#define TEST_EEPROM_PATH "/tmp"
#endif

#ifndef TEST_EEPROM_FILENAME
#define TEST_EEPROM_FILENAME "eeprom.bin"
#endif

#ifndef TEST_EEPROM_SIZE
#define TEST_EEPROM_SIZE 8192
#endif

#ifndef TEST_DIR
#define TEST_DIR "/tmp"
#endif

#ifndef TEST_FILENAME
#define TEST_FILENAME "tstf"
#endif

#ifndef TEST_FULL_EEPROM_FILENAME
#define TEST_FULL_EEPROM_FILENAME TEST_EEPROM_PATH "/" TEST_EEPROM_FILENAME
#endif

/**
 * @brief Generate test files
 * @param path
 * @param basename
 * @param num_files
 * @param maxsize
 * @return
 */
int generate_files(const char *path, const char *basename, int num_files, int maxsize);

/**
 * @brief Delete test files
 * @param path
 * @param basename
 * @param num_files
 * @return
 */
 int delete_files(const char *path, const char *basename, int num_files);

static char *test_files[] = {
         "Hello, file 0!wrbqhdrokyidsdrmwrsylbfacyedgxplrlnppfkokcqnnuwsmbucjismktxxvrbjtsfzfmfdrsfbnvhfsqwqaeczfklojpprxizxchkccedofddfgxqkydcdwtcoodqvcgpombaunyxzggptwlsduumqdueoyhahdmxdylnquwgljuwixbmneadmdaxohqmhvhovuopylemoezicspgbizruxmufkroziobpelpajaqdnwtjmppaxsughiqbjjvdsybemsqogxmeyzjgboffsdxisehczfirqnzqsbpysnpktdbobqwvfjjdngivgivcabepvghjebiuzzbuzasqquiwvdwvbrzgjfxtunssluuflbnkpalcijdszyeufcfoemjwgwkbehgcahsemphruydrbseyaobtnmwjsxkdrxrcdnovpxpdrfrqfgnrexnufpcgwxuyfcqnbmitclfzermevqdjqugnaqrjoxpwjbssfjexxnflwwnbjkmouhvgwjqxicoridhrschlehtmawwqsenfvwvjfzxcdnqjaokxgiecklogqvbsvvenqmrirmlbrkhynmodycguihexjroujuhdpzsygyqjhrryuzrnkhlfkebdpfijxhncmcoqndmzbnmphdtsqeeguismrgwrtadupzynr",
         "Hello, file 1!jncmkzdszodupnukumnfmscjaxrdyqczbvqqjtvnbaizwatzpmbjnvehzcpnumpljnewygfnxmsapdzmqxvqnblzgzmpnjlywxtonbiklskfcnmqlefnmuqoscoeyhgwoyvodfqwbpijmwplvcabbwbetwnnyvdxuqsabpthormfrckvbfhohnypbtrabdewpalhsttfslzuqsydtmrzqeehkkfpcvzsdcbiweyzftoksxgoxissfqjncdrluezmnunxlygluadyvaaslvcvimiwqskwxanniaebubqgxcrnxqlophoiammxvsafyncermxsjoegpqiqwrgrkhcihikmpsdgnxzswtcmawnnpdpulxkvrguerglkawbrmaieqvfhccjrbbgslquvevthtmxqvfpxwwjblzdcsdwqpuahgnaeoroqkxpzqlmobjrmxcbtovkjpsqxkuzoojicxbtmjnpvugaskfgtiqjllzmcmcedxlumaghfuvaricrfqwuqoesqrykhnjsxeyfuoqmytypaslzvedlgzjdrhdydndaswsxjwmfaxnjoimrrexlcfkvxqscxzwqiyapuuftoqnqixlsoadskgfxndlqmyetjikosqxtdqskvhawualdkdiyyeuzytjixmyokvsiijcytykj",
         "Hello, file 2!vskyawfcaxidxmtcqfbhavuajfwvuedffysilagaajtslefnytrlikpfvmugdxywznwxnwxusknwdaiaoaqqklqdmwgpoonghohfvawtslkjtxpmlxkntrubelorwqtlhcozgbeppcynmmholikibtmtkisjcmwbsivvceatywbxcplhekhvxbdjctoicsqvqvplvonagsooahfnnyetaziczkwhgqwkqpuvtlzrjobgmugtqtuinqinkdawmxaqedcnjemnunbjdhevnkohcqeqvwoorwyhsdqdvmzzftfogvdrvdehccxxzzbfznfcbqmjtrpxiwoseulndudioqsvolvrkklidhhgtdoqvejsmpcneqokwyqruevwcgopsgetkpioffuirfmaemdnzpklbstwusanfohxfpwjsxgawjpexeyosxhpopfxkcgucmcgimqtqbbaiolvjomkcndzqragwaevhejppjlolyzaacaumdpmybhpuktqmauvvhvushkwmcqdqpzxnizgatlwvhvppatnerczmslbovwxrbvtoeltzzbsmcavzepvkthvctywbuxussxmrfwizfpkbtazobjzxcjwqjgzckjzcpspvlipnfufydajgspofrklhvwztfibsmfydzzpejoyoewm",
         "Hello, file 3!akaixsqnitnjjrvwobhjkocericabgdtnjfcwtunxvqoqozpdjrccowmzxehitfvhxqqytitnxgobxqvzcumzqntetycrkulzliofvtugmagqdzhdutystkmdvpxttqsvhapdfxfqgdohshtctmuqesrmrhooovdnbhqwpgbijafipefqrexgtvgqwpjzduvqkzzwupasabhmewtqlnbdgzcecniygqlfchgajttnijchdlfommmhoidfxfxllvoajwbuhbzzwytzixgapodkmwtidotfieeggqfvnfqbxuczfckhhdzhrwcfowcebtsgcpismnqsvykgmyfiqnzdzciujcyilvpzfhvsdjrraxelqjmqyzjnpaejqeqoarjjgacgnydzpqwpfopxgnoadtjzzizwiuivqzfhdqxmjtwocdddkzfzkuhgfbthcinbqpjggfminvadtkcjnqrucpnjiklpdpoyzdpsbjjszfrrycfqtytedkggiskysydfkhyzaqprmzvkymzayrwaegrtidssxwzlkvxgqaloobiccgabatwdnvzwcxwraetngtiscwasvqssjxfatruyadqgodgbvwlouhtoeyoiblkaniwhdhzspauiyijqiawwcydjizuccewlxkwzocqnxeyovorvqlqqlzgecatdhg",
         "Hello, file 4!dncbuagxkacngbmyuasnkeuodbubudhysbebfjggqvamcdupbyhwhogzghsdmijupuwnzuccqriheeqesiqxywmmppevrazsekvnazmrucgjyirtbrnxrcljcelkcluqkupmpojimydjeaztzijymdlmzwwjdabqlqxyeipoituioefswqbftwnxzdwtvettilbplfatwtlflkhxpegqjpajitsdkltoyenxphyqxdkvvtvabzankjpumdhyluacaryrdxdgrrmfigxnlbnwvlmteidspvkoqhkxajauqbrgvxncajqvpdiabtwbealwoaovbxzlywhpzgranmyhllbofxkcbwpcwgzwryfvyzguldodxjykoluecfgrruculakdvrzncmuedvajuyxcumtyuisbmfndangslvgbhqxzqbvpjvuqvlidaunlciuucvebbdedcgtuiuckeasotfbggxgyuuifjbnvhjlscugyoezlrmbqmxosgcisxthiqihezvticpnhhkgpprxucoezitzjexmkoabmldyytgtdljocwqyfeblksaimfssyzqsjgalbdsjgdlvnyvepsgcintioakivgbzwfxookalipltkclbtopzhqeaqiumcqsuaeoyrirkemxdpmmvvghkuvewcjjbjiznhgamkjpaowyynfololkljn",
         "Hello, file 5!nqqzqfopfwakyjuuglcqstsrxzccrxwxcfuyrxbkgdupsadvknxuecilxcvidfkqcarrniinjchtvwbcuhymwjropiblbdykkiuxswhlbvwaqelkkhpwotfsakwzrewfhoujgiltsnrctsxblddrtxfuyoxdbkuefoyxhypkcisuktvlingchcwooclcfbufpofmuodtdaukzgczfoicqvypmsuktqdnjzdtwtiacdzuhogyhgdiuymxsntmvqguobeuqdstehrbyshvpokvddclghnnbvugrigegfhrkojplvfszafqnuqlldejdwihqqnlhyuamcwgsmvofmpxfwjhcpxbcwkmqkxgicdrvhgoykucyvyaiaowailshwdtnctsfsngrvorvdoemojlelsfkttabocubytaqqhyawdjdwvychkvxplbfjxpuihmhgewrhaqiutjzfcpogewrapbthhekpfzepkgrvakwjiocijdoaecogajjgjnzuedqtkuzpuatzdrmqklybgjzhaaocoxiskwptubgqpeqadiofjidthvgweelffzrvmalsskdykgkkckuvpzqwjloyoxuqvtmalfwqiunxadetytdpwvvbrcpjwnkdvursdlcekzjtzhrtkpysydrwrfkgjsdqaqpdprsmpltjlgrbfudmhbvli",
         "Hello, file 6!bwfotzskwbrscepbyabccmumgiwrjpcsrgkqmjaznfjmusvxggqgbmpixqmbhfhskhyghdidepzakokikyfmanerocwajeztevzazliqipyrifjwsbasqhhaszicpfjpuvtwuncuepnhzcnmmeztfxvgnqmcmyclwvrqzhfyktongbcyfvmwbltacfvshlkjeouhsdhubbtxpmtqesrxxxfqsclqlbbgyhcbgguupcafwscfzstewniehbavryourukmkyhtlyfmjpprbvlshrvhgzyjaknanbfxohautwjfjatmbjlzxmuiekpyhwejzpapwxnjmjmbkwjxbzfztwbbcrwlsxsplvhvskzlytydqywegbvqrjouxlxsqagzgbcpsmtxzsznahvukfphvsdjaansfwhgqhkriybpmdhpwpfaigybjulpdxqwbwkgioopqjwbtnodzzyfypkjrevrharmfcxocimpoucaqgiahwkdtbhjpjreorgpvuyulzpqqznqpbakgfeyymjvgizxilthdgodfevqwwzmwtxtqmjvihafzsbjfrifqhkqfyyejxgjmqswrmjweacbezkxnumuwhlacvgtywdilcsfeauiafdstnlrxypisnkncsuiaksmwhqibztvhf",
         "Hello, file 7!ozbebccgzwsreowsgyinbzgbyfjxpjcyzseszmzqpqupyvelunofzviyjxbomdizvdswdshmgvxkygdmsjegdyrbthlfgtrlbwfqduavwnwuepxhyekpmmijoqrpaxhwmnbonekdokiggilwjyobqaxaaumuehltsihktapdpdzwdxmmjmkqjvutzoxesrumewytgkqldnlbwmbyyvlhetzlhftachmnfmbkpelwppkzeuqgtoocewxmwyuuhtzxuokoajhnalkywtvpeaxgdynpynfdebssznjxnyirnqryuzvlmuvjomuclbnbeuwtffgmvshyoyejehnlryfvmujpvcqhucsegkoruxegdcyvbekelfxejyqrrrfkjcypznsqutxgjgkxgrjqahuyrbghggfofrmbklkxewheuiqobqngdgctdxuborhenanzzimmtbngokfjkuusdxehuoqmuenubznoviwadbkzljtawructrelhadbipraqzxdqloemsifhwnqnrvisirguutxwbrpjwfsziqxpqqllahlzapgxkfyygehhtaksjgcgrwjgqjbhlfjjkktbdcvdukjtbuzftxhzzaljucpbxczhyjhtoezhlaiqzfcuwkrmzhgxbtasebnkcpaavdxxzlcdcoecxdyaoafnqhbatyqiyortvstiddgnxbsfwpktci",
         "Hello, file 8!txnrvsuapbgultksyoxibckcwlvuhfgzpuxggxnufpmibomrdrudktcjsnmbmyheemzzsijfujrudfjwjoatlqvxrvsnkiemqzunuafggjtwtcmpsujupwtuicejhzdximdbcqczsngymlqsculaunekuvzqshljeucgnxlqkieybhhayevajejdhellnbmicpbhrkhwdfhfmrkmplthgniglcepqshojqurrrrpnxbmvujcmxtkuofpxmeuegciebxlrgfppedwqynkqqzjenprkzxybjeihkdykxvenvezpvdqgsvyjimjulafuojkhfykbnhrfcpakjsnzeyilqvxojjozrutysrxxyzvzsrejkbweyqcnhaclkgtnsksgonnusbzvsestkcuuiaoutfkgesdlotaszirwbhfszkvskudvjrhpuyllbqslypwdriojxiawtdxtvwxaypiujqdobmhwswufhiywkwdndfhqxqnoulbfycniserblkrvujtnpviwuzyewnmbvjagmviwtqailmhjctquebjryyezfqcutjrowkqmiqvferupbyfpflcnkcvhekmpehcqagzsxqdxyjmapcldwqdgewvsejfivphucyxkenfyldsnayxljjujddevkhnszynaxeftmuzjplqtzqclbuftlmftpamykeioitxicusfwqayhatczrllyyvam",
         "Hello, file 9!oepadjtyrfzfjuyiwouuielvfkobeejrzuwrtibjjrkbpvrgtmrzcrpnvczyiqhsztjgbsavkfabzkpiimxgrvfroeczajniyqoxyzkwppjvaablhwcenfyxhigbowotoukrfydumalbhthtdopqgenwgucuvgdcrzzihtnwpmqczulmqlebihlrzuohfrzvsiejfjxvgrnkbnlttcnlenxdzzvkpbifzazjjjhllywgfpeqisfnaxmtuhbiwwwghoadzkkmwyiggjujoqzruphmyftorgqppvyurbogbwaymexjaeuyjonnjnzctuedljjxhtjnvdnnnynanitrswxvlsfgmqjadvpycmiuhwrimhqttsgtojossjusudjhrnsjofbaohfsbnwmxnkarchqokzvakglayxmiltoycbzibivnyvaasoefohlxibtaadzobdtptypvffmscobxjlnoaokrhpfhfmenouajxwlaoozcyodscvyjggxnyhbfcezbmjeiafxmvwhyavcewbjnbkmxuhypmzifwgtrqxclqabndevrrzwgtryvyenpkeezxfqmrikfamiixalzcgtfycswxipaeseczedgcpuozzqgbottbqmkhkqwzhwoqkufcfsetgmaafzyzbntchszzxjdjmdzvyjcujzzroaocvmmxugvloqlqlvbdisbjzvratwbieoddm",
         "Hello, file 10!nvrkfonkibdlkotmewxiasfifmrwhdzjacodlfcfxsqsdanjvpxbsavfyjqqzxgdtmpdvhljmdyyongxpvypcjlcseasdkalqwfjmzqzgamxabruvyjndxqddiyhrebtuadacdvumtlkpqpoeofqqfcpfaozbvaebnlrbqxwtwjnlfutdcntrzssusnsqoihpffwjnsoztqoluklnfquacphbcqzuawgghqyklxqzrkunkkyeqintuxrendufdxjkzpmpeukgqifshnwrrlumovixqkxhxztszritoxtxgiplljdsiarrihooqzntzgfjkieltsbehxtvvxcstueociswuwoiylptjhwnffkyzwtynvsmdqtabzkuzzwjlnviwafujuwegndleyzahrdxihjbrtmnesteurkojymhpcgvathywgiaukzmwtfrgcltvpwvpzcgbfrnmnugkvoyaxifgrnzyywkdoxykeagyignvbuasvfrrnhavvrkxhyvaijxxugqgfvndoewzninzofhskgluqlivxkkwrgnpqzhdvmadmnzzltidgcynvzixrswmzkqislrlbjqfvdubwfbmblailncnxpjmgwiejfpnheajkqtbykjuhfrxajvzwavteewjbalsdnbfdokwpdcocatzbxbbkgbihdztkqreewhnoozhmueolwvyfivgsdwcxunuq",
         "Hello, file 11!oebayyrntmyayxbenhniivfjjdikohbiyhawxhccoujhoqfhrrgdhifusuubvdufchkoxaiymfwtctmlvzfalfaxyvmmpqgsvnfgunrhojvtfnmralxaeozkcstjxohtoozheztvgvdcijbwlfbpmmcdzmkekjudttzdatynbdtfzpvdfmiseqmehbrgxnrjrekpoxkrkwbxbqerxbwavoadysfxbdrgszyczgvonuxdljcrzksweslvqvlngbnfnneynkbzyeuktkhyjbwxulqpidrgctkgeilnsdlcpyzrapegipabdganswuubhkshnlewixmiizzrixvjrdliwqxucltkffqonftuxzldorunkflknyfxfzrguqarypsjrnadizjieuevkpmjfgiqotxadkipswemdvcdepdfmsvqquvkbjuwobeaxrfkaumuouukgcvhyojbcfdfgevndqvsxyfsyaayijtkpwafvgflaitkkkuewwpwhmhvmlnlsfralmbnrabiaxsogzgpzvqmmgjfmakpmsslnvtnxxmtgellnqoznkfqtktharhkleigcgpmlrcfkodsfsflqrpqbiiilqzhitaxzavsdenimritcdtyqjumemgxuiruqzahygwtmzgnohyccdzveuygxddhpbshfzwhadrksuirsnxhgkchvcydbobjzf",
         "Hello, file 12!mbnzpbgzsxersktljcxcfnvfdfmukymmzvskflkrvyxpyoalszhmgspilwdsetmfmtpkeqohkkttpdsiqcldqmweayvtglhhafzfguolrlcybjijdxztwjrchzrujgzdhginsrvxinwxrbyvjofyckldgxbmblxhlaassdnayauwfzpldxojvnufotviyfougyjfvqjjhfwfymdizgrphfinvukpozldswxaxwyxgovdjdlhurraawkvoktypwmijburaxkeimcfowiigwnaebwqviptsfwxbiifleppampxynucenwfcpjficcjeyjxmhepyznscxwhgchilbbvyjfjznoaemeflfarznenebzrfsczicikwnwgfzmmybipidpynospdfhichndsmvvbtdsyzumtjucuwcydzdrslzmvblguxwfrlyaveorsdylnuiofxdwozyrluheshbbynbeovyfltjpyevijxldlqotwuokinyzxvdkyzxojbslgfyqcimxuzqnyyesmsekwrnumdmijdsbxqepymyxcstbhtdhqfcyemsomnhoutebzuccckcugayhzsccbgrlbsxxwvpohtaowhekumrykhanfugjerqsgcqnyztixfohkfdkbmruuqipfkoqtaslbpwkeibwvbajoszeaydsnycwcktygqwjnxtcsuywsfcuwsgskpfkw",
         "Hello, file 13!wsvhcfkfwzvhumyswykcwczpurdrcvcazvizhmodrbwucbhfcsasypkyqwvkjcpzmfnveagrbihwglkrgkwgtslxzmdalkavabqbsmgtsmrhcbvkhywiyxfjxxmvyuzurgxhnzwwglzjjwkqoupiharapvyqmrgbifydwmcrbkvgoejqeqmlablwhmjmwvawijjeumbwtngcrilyxsfsgjruqpkdsxtdgzvnanfgqwqemsyaurvueprkvkkztgrxmdxfjpujkelsaxmfniipltiyyqlsmfowuptxnnvstisrsekjhlehdqlxrkgrcgyupjsopwmfarwjgnhlpsofkgyfgkjajuxodypyjbvlgfpixsmlsirjomumvtanhvrlssfnqgzolhmlfpphzudbjhdegdjnxmivupzwbsaukaoluaunprwysbpyvxnxkmaxegdaxnbnlkvljlbbzjrapcdteqngckfmyligevcwuirhnagajwatmlfwaixoncccreaxoduugpyovgxkijqzrceidpeoyahqdciaemcuwhresqtsgsypxochvenfrkcjayycvzcyamnxqrnfgovcjnjmqyskhdgnntuawscqcyrhnxujquaqnemlpqthkftqsviruovaltprjgmpjbwqragaubylplplmsfcnlgqjnlqrgfggajlgumq",
         "Hello, file 14!gquadcrawbbhuoomsdzmmynuqkvtqzwbqdjtjaxjshqhmmtxtdszsbwfjdqhunlaplcnmodahpsewbephdpiknpesxwzwbfwvyvncvntytunvvhpupvudsszdmmuqkieotziivnusvkrfddugbfjmrfuwffizbvuligidwnkirnborcpmhuprjerndlsklzbdiaylxffyyqlbpbqedwypbmrrwfljlxrckckfqvezunrihgjyldujevtmdvvzmissizispwkxhmzlvlbtdsdzzefdyvgnauhniwtjvfspdusqiwzcrwxeuvyhyqjjslqbpwfhrmovxeteuhgzhjaowlyyuesdbcueysiuirmfxvwgkhnkdpkvrdglwxjtqcqtqjuutqihnajuyfiolagsscnjrziepbmzqevxgtlvqgplyhdjhqlvpouigncemmunkiuovpofnrtshzcgtkuzdinnhuckuoznlhxyztxrphzeesoxcipcqegrjmtckacbgjrmluqnohufjijuumrutwbimzepnainarbybgewgbdzqpxffrmelubkouclvufsaucbjfpwbznzpfxhklcclaxhqgtrefwbrvfkeunfljtpzbkdszxqyipqagettayhgrskwfjnzhtjzsqtievukfwhrcvzzvvo",
         "Hello, file 15!uxzvxxajboxoimvaxbrzpkvkslnigtruuntfnakfoutwcemjvyozvmologmkwlqwyzyrdcawyyifarqbiceezaxqclbqjuvpaonpborywfabflsiubrvjurvenzsjkhztmvgtgvbfidmhusswxpygvkdlwogilwomormubgyqjjdsuxlzppqnsvorvqtqcbbrsuszcflpyitkrmnmkqqpolvhkqtcneujyjtlzhcrkuhoqykrldelqblkbyfghkphhedrkomsfzikpnxhjrghdqwhdosdspnxmkxnpvnnleboymfjyokagoypfrdcmqamuxgivkcjxpkfkqwmhyjbhepsqodyqbxiztqrnjlrlbdevmvzdhrvwjhllirougymaxuaacpdfvbmnqapavjokuhekmklsiiunepklbbigbwnulymankrcsavbznrnlkcpcpxqavauwosssetmusvkctdbapvbqkagcojxfpjnlfqwnrhjbjyyarhukahiilcbursbfdciypkddjknycymbumrprivkuveomnsqtsptlfqsnuhbxxefujkvfgsgoclvtciyiqkponysymywctjqzlsearppfypzwflyczlhyplamwyupbuokyulggjkjyeftxjqatwmhxdptyjtotdvazuckvmezejmsxuditvgxvoqdyxmunblynbaxmw"
 };

#endif //JEEFS_TESTS_COMMON_H
