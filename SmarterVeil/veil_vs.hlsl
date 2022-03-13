//float4 main( float4 pos : POSITION ) : SV_POSITION
//{
//	return pos;
//}

float4 main(uint vI : SV_VERTEXID) :SV_POSITION
{
    return float4(2.0 * (float(vI & 1) - 0.5), -(float(vI >> 1) - 0.5) * 2.0, 0, 1);
}