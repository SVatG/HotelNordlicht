#define numVerticesIcosa 32
#define numNormalsIcosa 60
#define numFacesIcosa 60

#include "Rasterize.h"

const init_vertex_t verticesIcosa[] = {
	{ F(-0.0988196), F(-0.375466), F(-0.0962288) }, 
	{ F(0.2972004), F(-0.2676744), F(-0.0043744) }, 
	{ F(0.0039648), F(-0.267768), F(0.2971212) }, 
	{ F(-0.355828), F(-0.1298656), F(0.1285332) }, 
	{ F(-0.2849472), F(-0.0445456), F(-0.2771592) }, 
	{ F(0.1186444), F(-0.1297164), F(-0.3592964) }, 
	{ F(0.2849472), F(0.0445456), F(0.2771592) }, 
	{ F(-0.1186444), F(0.1297164), F(0.3592964) }, 
	{ F(-0.2972004), F(0.2676744), F(0.0043744) }, 
	{ F(-0.0039648), F(0.267768), F(-0.2971212) }, 
	{ F(0.355828), F(0.1298656), F(-0.1285332) }, 
	{ F(0.0988196), F(0.375466), F(0.0962288) }, 
	{ F(0.1045448), F(-0.470636), F(0.101534) }, 
	{ F(0.1637956), F(-0.3993096), F(-0.2376148) }, 
	{ F(-0.2328532), F(-0.3994348), F(0.1702032) }, 
	{ F(-0.3821244), F(-0.2841032), F(-0.126508) }, 
	{ F(-0.13698), F(-0.2840264), F(-0.3785536) }, 
	{ F(0.398698), F(-0.1382204), F(-0.2543052) }, 
	{ F(0.3028252), F(-0.2536296), F(0.2944512) }, 
	{ F(-0.2430952), F(-0.1384236), F(0.405558) }, 
	{ F(-0.4846208), F(0.0481864), F(-0.0745304) }, 
	{ F(-0.0879712), F(0.048312), F(-0.482348) }, 
	{ F(0.4846208), F(-0.0481864), F(0.0745304) }, 
	{ F(0.0879712), F(-0.048312), F(0.482348) }, 
	{ F(-0.398698), F(0.1382204), F(0.2543052) }, 
	{ F(-0.3028252), F(0.2536296), F(-0.2944512) }, 
	{ F(0.2430956), F(0.1384236), F(-0.4055584) }, 
	{ F(0.3821244), F(0.2841032), F(0.126508) }, 
	{ F(0.13698), F(0.2840264), F(0.3785536) }, 
	{ F(-0.1637956), F(0.3993096), F(0.2376148) }, 
	{ F(-0.1045448), F(0.470636), F(-0.101534) }, 
	{ F(0.2328532), F(0.3994348), F(-0.1702032) }, 
};

const init_vertex_t normalsIcosa[] = {
	{ F(0.145780084454015), F(-0.894939271451477), F(0.42170115886784) }, 
	{ F(0.425853695646406), F(-0.894854599793235), F(0.133730606568535) }, 
	{ F(-0.312461772319193), F(-0.798239941594143), F(0.514956926822941) }, 
	{ F(-0.858844643176977), F(-0.509881804502446), F(-0.0490553190137429) }, 
	{ F(0.128085955893904), F(-0.900930832456935), F(-0.414629500917626) }, 
	{ F(0.0476148785827558), F(-0.997805976534897), F(0.0460006144392749) }, 
	{ F(-0.41063295069631), F(-0.901102396329949), F(0.139266116233872) }, 
	{ F(-0.613374014963569), F(-0.744458405056825), F(-0.26373092520165) }, 
	{ F(-0.280423829476861), F(-0.744352518848364), F(-0.606054290922545) }, 
	{ F(0.654819392783143), F(-0.311617633296895), F(-0.68855356614686) }, 
	{ F(0.69515381546546), F(-0.600121457606532), F(0.395746647445086) }, 
	{ F(-0.326367322258765), F(-0.443745541275842), F(0.834610247696165) }, 
	{ F(-0.99804960261513), F(-0.0585919249000005), F(0.0215401266545608) }, 
	{ F(-0.391641472784252), F(0.0230743029132545), F(-0.919828534750064) }, 
	{ F(0.942055428958669), F(-0.321108798332977), F(0.0970603338272151) }, 
	{ F(0.123262756421967), F(-0.321363108603255), F(0.938899379757017) }, 
	{ F(-0.881349560376977), F(0.0636939649887832), F(0.468151718193276) }, 
	{ F(-0.683449398569649), F(0.30193055699071), F(-0.664631370272322) }, 
	{ F(0.443471531034723), F(0.0641140007112944), F(-0.893992391508178) }, 
	{ F(0.335783578435436), F(-0.666207450232809), F(-0.665895653768211) }, 
	{ F(0.683449398569649), F(-0.30193055699071), F(0.664631370272322) }, 
	{ F(-0.656105567269697), F(-0.666524368157898), F(0.353936083565132) }, 
	{ F(-0.458205434601532), F(-0.428280499610436), F(-0.778860445365572) }, 
	{ F(0.881349560376977), F(-0.0636939649887831), F(-0.468151718193276) }, 
	{ F(-0.443471531034723), F(-0.0641140007112943), F(0.893992391508178) }, 
	{ F(-0.791146009188881), F(-0.428393315895732), F(-0.436539985614567) }, 
	{ F(-0.942055428958669), F(0.321108798332976), F(-0.097060333827215) }, 
	{ F(-0.0727247480428935), F(-0.509637750082383), F(-0.857310022519896) }, 
	{ F(-0.123262756421967), F(0.321363108603255), F(-0.938899379757017) }, 
	{ F(0.506333150547357), F(-0.797979058180763), F(-0.326888610021412) }, 
	{ F(0.326367322258765), F(0.443745541275842), F(-0.834610247696165) }, 
	{ F(0.93035192024696), F(-0.0229114639293814), F(0.365951320961417) }, 
	{ F(0.99804960261513), F(0.0585919249000005), F(-0.0215401266545608) }, 
	{ F(0.0061623202777849), F(0.0582626154003719), F(0.998282271432035) }, 
	{ F(0.391640606516049), F(-0.0230746265489592), F(0.919828895467721) }, 
	{ F(-0.825357597223426), F(0.443380477797548), F(0.349569147113401) }, 
	{ F(-0.654819392783143), F(0.311617633296895), F(0.68855356614686) }, 
	{ F(-0.415072361890889), F(0.600218103340982), F(-0.683705464959929) }, 
	{ F(-0.69515381546546), F(0.600121457606531), F(-0.395746647445086) }, 
	{ F(0.670019244807523), F(0.312038223374125), F(-0.673577284905805) }, 
	{ F(0.613374014963569), F(0.744458405056825), F(0.263730925201651) }, 
	{ F(0.415072361890889), F(-0.600218103340982), F(0.683705464959929) }, 
	{ F(0.280423829476861), F(0.744352518848364), F(0.606054290922545) }, 
	{ F(-0.670019244807523), F(-0.312038223374125), F(0.673577284905805) }, 
	{ F(-0.128085955893903), F(0.900930832456935), F(0.414629500917626) }, 
	{ F(-0.93035192024696), F(0.0229114639293814), F(-0.365951320961417) }, 
	{ F(-0.0476148785827558), F(0.997805976534897), F(-0.046000614439275) }, 
	{ F(-0.00616232027778491), F(-0.0582626154003719), F(-0.998282271432035) }, 
	{ F(0.410632624613), F(0.901102689548234), F(-0.139265180473721) }, 
	{ F(0.825357536458597), F(-0.443381912990139), F(-0.349567470231714) }, 
	{ F(0.312461772319193), F(0.798239941594143), F(-0.514956926822941) }, 
	{ F(0.0727247480428935), F(0.509637750082383), F(0.857310022519896) }, 
	{ F(0.858844643176977), F(0.509881804502446), F(0.0490553190137428) }, 
	{ F(-0.506333150547357), F(0.797979058180763), F(0.326888610021412) }, 
	{ F(0.458205434601532), F(0.428280499610435), F(0.778860445365572) }, 
	{ F(-0.145780084454015), F(0.894939271451477), F(-0.42170115886784) }, 
	{ F(-0.335783578435436), F(0.666207450232809), F(0.665895653768211) }, 
	{ F(0.656105567269697), F(0.666524368157898), F(-0.353936083565132) }, 
	{ F(-0.425853695646406), F(0.894854599793235), F(-0.133730606568535) }, 
	{ F(0.791146009188881), F(0.428393315895732), F(0.436539985614567) }, 
};

const index_triangle_t facesIcosa[] = {
	{1, 0, 13, 0},
	{0, 2, 14, 1},
	{0, 3, 15, 2},
	{0, 4, 16, 3},
	{1, 5, 17, 4},
	{2, 1, 18, 5},
	{3, 2, 19, 6},
	{4, 3, 20, 7},
	{5, 4, 21, 8},
	{1, 10, 22, 9},
	{2, 6, 23, 10},
	{3, 7, 24, 11},
	{4, 8, 25, 12},
	{5, 9, 26, 13},
	{6, 10, 27, 14},
	{7, 6, 28, 15},
	{8, 7, 29, 16},
	{9, 8, 30, 17},
	{10, 9, 31, 18},
	{0, 1, 12, 19},
	{1, 2, 12, 20},
	{2, 0, 12, 21},
	{0, 5, 13, 22},
	{5, 1, 13, 23},
	{2, 3, 14, 24},
	{3, 0, 14, 25},
	{3, 4, 15, 26},
	{4, 0, 15, 27},
	{4, 5, 16, 28},
	{5, 0, 16, 29},
	{5, 10, 17, 30},
	{10, 1, 17, 31},
	{1, 6, 18, 32},
	{6, 2, 18, 33},
	{2, 7, 19, 34},
	{7, 3, 19, 35},
	{3, 8, 20, 36},
	{8, 4, 20, 37},
	{4, 9, 21, 38},
	{9, 5, 21, 39},
	{10, 6, 22, 40},
	{6, 1, 22, 41},
	{6, 7, 23, 42},
	{7, 2, 23, 43},
	{7, 8, 24, 44},
	{8, 3, 24, 45},
	{8, 9, 25, 46},
	{9, 4, 25, 47},
	{9, 10, 26, 48},
	{10, 5, 26, 49},
	{10, 11, 27, 50},
	{11, 6, 27, 51},
	{6, 11, 28, 52},
	{11, 7, 28, 53},
	{7, 11, 29, 54},
	{11, 8, 29, 55},
	{8, 11, 30, 56},
	{11, 9, 30, 57},
	{9, 11, 31, 58},
	{11, 10, 31, 59},
};
