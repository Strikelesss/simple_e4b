# simple_e4b
 
A C++17 header-only Emulator EOS file reader & writer.

To use it, all you need to do is copy both "e4b_types.hpp" and "simple_e4b.hpp" to your project and then simply include "simple_e4b.hpp".

Code Examples:

Reading:
```cpp
#include "simple_e4b.hpp"

...

simple_e4b::E4BBank bank;
if(simple_e4b::ReadE4B(SOUNDBANK_PATH, bank) == simple_e4b::EE4BReadResult::READ_SUCCESS)
{
	//  Use bank result ...
}
```

Writing:
```cpp
#include "simple_e4b.hpp"

...

simple_e4b::E4BBank createdBank;

// Create a preset named "Untitled" with no voices:
createdBank.AddPreset(simple_e4b::E4Preset("Untitled", {}));
	
simple_e4b::WriteE4B(SOUNDBANK_WRITE_PATH, createdBank);
```
