/* Plugins.cpp
Copyright (c) 2022 by Sam Gleske (samrocketman on GitHub)

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "Plugins.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Files.h"
#include "Logger.h"

#include <algorithm>
#include <map>

using namespace std;

namespace {
	Set<Plugin> plugins;

	void LoadSettingsFromFile(const string &path)
	{
		DataFile prefs(path);
		for(const DataNode &node : prefs)
		{
			if(node.Token(0) != "state")
				continue;

			for(const DataNode &child : node)
				if(child.Size() == 2)
				{
					auto *plugin = plugins.Get(child.Token(0));
					plugin->enabled = child.Value(1);
					plugin->currentState = child.Value(1);
				}
		}
	}
}



// Checks if there are any dependencies of any kind.
bool Plugin::PluginDependencies::IsEmpty() const
{
	return required.empty() && optional.empty() && conflicted.empty();
}



// Checks if there are any duplicate dependencies. E.g. the same dependency in both required and conflicted.
bool Plugin::PluginDependencies::IsValid() const
{
	// We will check every dependency before returning to allow the
	// plugin developer to see all errors and not just the first.
	bool isValid = true;

	// Required dependencies will already be valid due to sets not
	// allowing duplicate values. Therefore we only need to check optional
	// and conflicts.
	for(const string &dependency : optional)
	{
		if(required.count(dependency))
		{
			Logger::LogError("Warning: Optional dependency with the name \"" + dependency
				+ "\" was already found in required dependencies list.");
		}
	}
	for(const string &dependency : conflicted)
	{
		if(required.count(dependency))
		{
			isValid = false;
			Logger::LogError("Warning: Conflicts dependency with the name \"" + dependency
				+ "\" was already found in required dependencies list.");
		}
		else if(optional.count(dependency))
		{
			isValid = false;
			Logger::LogError("Warning: Conflicts dependency with the name \"" + dependency
				+ "\" was already found in optional dependencies list.");
		}
	}

	return isValid;
}



// Checks whether this plugin is valid, i.e. whether it exists.
bool Plugin::IsValid() const
{
	return !name.empty();
}



// Attempt to load a plugin at the given path.
const Plugin *Plugins::Load(const string &path)
{
	// Get the name of the folder containing the plugin.
	size_t pos = path.rfind('/', path.length() - 2) + 1;
	string name = path.substr(pos, path.length() - 1 - pos);

	string pluginFile = path + "plugin.txt";
	string aboutText;
	string version;
	set<string> authors;
	set<string> tags;
	Plugin::PluginDependencies dependencies;

	// Load plugin metadata from plugin.txt.
	bool hasName = false;
	for(const DataNode &child : DataFile(pluginFile))
	{
		if(child.Token(0) == "name" && child.Size() >= 2)
		{
			name = child.Token(1);
			hasName = true;
		}
		else if(child.Token(0) == "about" && child.Size() >= 2)
		{
			aboutText += child.Token(1);
			aboutText += '\n';
		}
		else if(child.Token(0) == "version" && child.Size() >= 2)
			version = child.Token(1);
		else if(child.Token(0) == "authors" && child.HasChildren())
			for(const DataNode &grand : child)
				authors.insert(grand.Token(0));
		else if(child.Token(0) == "tags" && child.HasChildren())
			for(const DataNode &grand : child)
				tags.insert(grand.Token(0));
		else if(child.Token(0) == "dependencies" && child.HasChildren())
		{
			for(const DataNode &grand : child)
			{
				if(grand.Token(0) == "game version")
					dependencies.gameVersion = grand.Token(1);
				else if(grand.Token(0) == "requires" && grand.HasChildren())
					for(const DataNode &great : grand)
						dependencies.required.insert(great.Token(0));
				else if(grand.Token(0) == "optional" && grand.HasChildren())
					for(const DataNode &great : grand)
						dependencies.optional.insert(great.Token(0));
				else if(grand.Token(0) == "conflicts" && grand.HasChildren())
					for(const DataNode &great : grand)
						dependencies.conflicted.insert(great.Token(0));
				else
					grand.PrintTrace("Skipping unrecognized attribute:");
			}
		}
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}

	// 'name' is a required field for plugins with a plugin description file.
	if(Files::Exists(pluginFile) && !hasName)
		Logger::LogError("Warning: Missing required \"name\" field inside plugin.txt");

	// Plugin names should be unique.
	auto *plugin = plugins.Get(name);
	if(plugin && plugin->IsValid())
	{
		Logger::LogError("Warning: Skipping plugin located at \"" + path
			+ "\" because another plugin with the same name has already been loaded from: \""
			+ plugin->path + "\".");
		return nullptr;
	}

	// Skip the plugin if the dependencies aren't valid.
	if(!dependencies.IsValid())
	{
		Logger::LogError("Warning: Skipping plugin located at \"" + path
			+ "\" because plugin has errors in its dependencies.");
		return nullptr;
	}

	plugin->name = std::move(name);
	plugin->path = path;
	// Read the deprecated about.txt content if no about text was specified.
	plugin->aboutText = aboutText.empty() ? Files::Read(path + "about.txt") : std::move(aboutText);
	plugin->version = std::move(version);
	plugin->authors = std::move(authors);
	plugin->tags = std::move(tags);
	plugin->dependencies = std::move(dependencies);

	return plugin;
}



void Plugins::LoadSettings()
{
	// Global plugin settings
	LoadSettingsFromFile(Files::Resources() + "plugins.txt");
	// Local plugin settings
	LoadSettingsFromFile(Files::Config() + "plugins.txt");
}



void Plugins::Save()
{
	if(plugins.empty())
		return;
	DataWriter out(Files::Config() + "plugins.txt");

	out.Write("state");
	out.BeginChild();
	{
		for(const auto &it : plugins)
			if(it.second.IsValid())
				out.Write(it.first, it.second.currentState);
	}
	out.EndChild();
}



// Whether the path points to a valid plugin.
bool Plugins::IsPlugin(const string &path)
{
	// A folder is a valid plugin if it contains one (or more) of the assets folders.
	// (They can be empty too).
	return Files::Exists(path + "data") || Files::Exists(path + "images") || Files::Exists(path + "sounds");
}



// Returns true if any plugin enabled or disabled setting has changed since
// launched via user preferences.
bool Plugins::HasChanged()
{
	for(const auto &it : plugins)
		if(it.second.enabled != it.second.currentState)
			return true;
	return false;
}



// Returns the list of plugins that have been identified by the game.
const Set<Plugin> &Plugins::Get()
{
	return plugins;
}



// Toggles enabling or disabling a plugin for the next game restart.
void Plugins::TogglePlugin(const string &name)
{
	auto *plugin = plugins.Get(name);
	plugin->currentState = !plugin->currentState;
}
