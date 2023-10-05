# simple_e4b
 
A C++17 header-only simple Emulator EOS file reader.

To use it, all you need to do is copy both "e4b_types.hpp" and "simple_e4b.hpp" to your project and then simply include "simple_e4b.hpp".

Code Example:
```cpp
#include "simple_e4b.hpp"

...

simple_e4b::E4BBank bank;
if(simple_e4b::ReadE4B(SOUNDBANK_PATH, bank) == simple_e4b::EE4BReadResult::READ_SUCCESS)
{
	//  Use bank result ...
}
```
