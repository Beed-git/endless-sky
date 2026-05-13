/* GameVersion.cpp
Copyright (c) 2025 by TomGoodIdea

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "GameVersion.h"

#include <charconv>

using namespace std;



string GameVersion::ToString() const
{
	return to_string(numbers[0]) + '.'
		+ to_string(numbers[1]) + '.'
		+ to_string(numbers[2]) + '.'
		+ to_string(numbers[3])
		+ (fullRelease ? "" : "-alpha");
}


const GameVersion GameVersion::FromString(std::string text)
{
	array<unsigned, 4> version = {};
	bool fullRelease = !text.ends_with("-alpha");

	size_t start = 0;
	size_t next = 0;
	for(int i = 0; i < 4; ++i)
	{
		next = text.find('.', start);

		unsigned value = 0;
		auto [ptr, ec] = from_chars(text.data() + start, text.data() + (next == string::npos ? text.length() : next), value);
		version[i] = value;

		if(ec != std::errc() || next == string::npos || next + 1 > text.length())
			break;

		start = next + 1;
	}

	return GameVersion(version[0], version[1], version[2], version[3], fullRelease);
}
