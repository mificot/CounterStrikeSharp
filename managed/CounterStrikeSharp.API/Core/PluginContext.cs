/*
 *  This file is part of CounterStrikeSharp.
 *  CounterStrikeSharp is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  CounterStrikeSharp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CounterStrikeSharp.  If not, see <https://www.gnu.org/licenses/>. *
 */

using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using CounterStrikeSharp.API.Core.Attributes;
using CounterStrikeSharp.API.Core.Logging;
using McMaster.NETCore.Plugins;
using Microsoft.Extensions.Logging;

namespace CounterStrikeSharp.API.Core
{
    public class PluginContext
    {
        private BasePlugin _plugin;
        private PluginLoader _assemblyLoader;

        public string Name => _plugin?.ModuleName;
        public string Version => _plugin?.ModuleVersion;
        public string Description => _plugin.ModuleDescription;
        public string Author => _plugin.ModuleAuthor;
        public Type PluginType => _plugin?.GetType();
        public string PluginPath => _plugin?.ModulePath;

        public int PluginId { get; }

        private readonly string _path;
        private readonly FileSystemWatcher _fileWatcher;

        // TOOD: ServiceCollection
        private ILogger _logger = CoreLogging.Factory.CreateLogger<PluginContext>();

        public PluginContext(string path, int id)
        {
            _path = path;
            PluginId = id;

            _assemblyLoader = PluginLoader.CreateFromAssemblyFile(path, new[] { typeof(IPlugin) }, config =>
            {
                config.EnableHotReload = true;
                config.IsUnloadable = true;
            });

            _fileWatcher = new FileSystemWatcher
            {
                Path = Path.GetDirectoryName(path)
            };

            _fileWatcher.Deleted += async (s, e) =>
            {
                if (e.FullPath == path)
                {
                    _logger.LogInformation("Plugin {Name} has been deleted, unloading...", Name);
                    Unload(true);
                }
            };

            _fileWatcher.Filter = "*.dll";
            _fileWatcher.EnableRaisingEvents = true;
            _assemblyLoader.Reloaded += async (s, e) => await OnReloadedAsync(s, e);
        }

        private Task OnReloadedAsync(object sender, PluginReloadedEventArgs eventargs)
        {
            _logger.LogInformation("Reloading plugin {Name}", Name);
            _assemblyLoader = eventargs.Loader;
            Unload(hotReload: true);
            Load(hotReload: true);

            return Task.CompletedTask;
        }

        public void Load(bool hotReload = false)
        {
            using (_assemblyLoader.EnterContextualReflection())
            {
                Type pluginType = _assemblyLoader.LoadDefaultAssembly().GetTypes()
                    .FirstOrDefault(t => typeof(IPlugin).IsAssignableFrom(t));

                if (pluginType == null) throw new Exception("Unable to find plugin in DLL");

                var minimumApiVersion = pluginType.GetCustomAttribute<MinimumApiVersion>()?.Version;
                var currentVersion = Api.GetVersion();
                
                // Ignore version 0 for local development
                if (currentVersion > 0 && minimumApiVersion != null && minimumApiVersion > currentVersion)
                    throw new Exception(
                        $"Plugin \"{Path.GetFileName(_path)}\" requires a newer version of CounterStrikeSharp. The plugin expects version [{minimumApiVersion}] but the current version is [{currentVersion}].");

                _logger.LogInformation("Loading plugin {Name}", pluginType.Name);
                _plugin = (BasePlugin)Activator.CreateInstance(pluginType)!;
                _plugin.ModulePath = _path;
                _plugin.RegisterAllAttributes(_plugin);
                _plugin.Logger =  PluginLogging.CreatePluginLogger(this);
                _plugin.InitializeConfig(_plugin, pluginType);
                _plugin.Load(hotReload);

                _logger.LogInformation("Finished loading plugin {Name}", Name);
            }
        }

        public void Unload(bool hotReload = false)
        {
            var cachedName = Name;

            _logger.LogInformation("Unloading plugin {Name}", Name);

            _plugin.Unload(hotReload);

            _plugin.Dispose();

            if (!hotReload)
            {
                _assemblyLoader.Dispose();
                _fileWatcher.Dispose();
            }

            _logger.LogInformation("Finished unloading plugin {Name}", Name);
        }
    }
}