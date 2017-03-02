#include "Testes.hpp"

namespace RF{
	namespace Testes{
		namespace Padroes{
			vector<float> GerarModelo1(){
				vector<float> keySignalSamples(32, 0.0f);
				auto off = keySignalSamples.begin();
				fill(off, off + 8, 0.0f); off += 8;
				fill(off, off + 8, 1.0f); off += 8;
				fill(off, off + 4, 0.0f); off += 4;
				fill(off, off + 4, 1.0f); off += 4;
				fill(off, off + 8, 0.0f);
				return keySignalSamples;
			}
			vector<float> GerarModelo2(){
				vector<float> keySignalSamples(32, 0.0f);
				auto off = keySignalSamples.begin();
				float levelH = 1.0f;
				float levelL = 0.0f;
				fill(off, off + 8, levelL); off += 8;
				fill(off, off + 8, levelH); off += 8;
				fill(off, off + 2, levelL); off += 2;
				fill(off, off + 2, levelH); off += 2;
				fill(off, off + 2, levelL); off += 2;
				fill(off, off + 2, levelH); off += 2;
				fill(off, off + 8, levelL);
				return keySignalSamples;
			}
			vector<float> GerarModelo3(){
				vector<float> keySignalSamples(32, 0.0f);
				auto off = keySignalSamples.begin();
				for (unsigned i = 0u; i < keySignalSamples.size() / 2u; i++){
					*off++ = static_cast<float>(i % 2u);
					*off++ = static_cast<float>(i % 2u);
				}
				return keySignalSamples;
			}
		}
	}
}