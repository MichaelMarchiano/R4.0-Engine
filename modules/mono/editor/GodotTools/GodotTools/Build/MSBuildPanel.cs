using System;
using Godot;
using GodotTools.Internals;
using JetBrains.Annotations;
using static GodotTools.Internals.Globals;
using File = GodotTools.Utils.File;

namespace GodotTools.Build
{
    public class MSBuildPanel : VBoxContainer
    {
        public BuildOutputView BuildOutputView { get; private set; }

        private MenuButton buildMenuBtn;
        private Button errorsBtn;
        private Button warningsBtn;
        private Button viewLogBtn;

        private void WarningsToggled(bool pressed)
        {
            BuildOutputView.WarningsVisible = pressed;
            BuildOutputView.UpdateIssuesList();
        }

        private void ErrorsToggled(bool pressed)
        {
            BuildOutputView.ErrorsVisible = pressed;
            BuildOutputView.UpdateIssuesList();
        }

        [UsedImplicitly]
        public void BuildSolution()
        {
            if (!File.Exists(GodotSharpDirs.ProjectSlnPath))
                return; // No solution to build

            try
            {
                // Make sure our packages are added to the fallback folder
                NuGetUtils.AddBundledPackagesToFallbackFolder(NuGetUtils.GodotFallbackFolderPath);
            }
            catch (Exception e)
            {
                GD.PushError("Failed to setup Godot NuGet Offline Packages: " + e.Message);
            }

            if (!BuildManager.BuildProjectBlocking("Debug"))
                return; // Build failed

            // Notify running game for hot-reload
            Internal.EditorDebuggerNodeReloadScripts();

            // Hot-reload in the editor
            GodotSharpEditor.Instance.GetNode<HotReloadAssemblyWatcher>("HotReloadAssemblyWatcher").RestartTimer();

            if (Internal.IsAssembliesReloadingNeeded())
                Internal.ReloadAssemblies(softReload: false);
        }

        [UsedImplicitly]
        private void RebuildSolution()
        {
            if (!File.Exists(GodotSharpDirs.ProjectSlnPath))
                return; // No solution to build

            try
            {
                // Make sure our packages are added to the fallback folder
                NuGetUtils.AddBundledPackagesToFallbackFolder(NuGetUtils.GodotFallbackFolderPath);
            }
            catch (Exception e)
            {
                GD.PushError("Failed to setup Godot NuGet Offline Packages: " + e.Message);
            }

            if (!BuildManager.BuildProjectBlocking("Debug", targets: new[] { "Rebuild" }))
                return; // Build failed

            // Notify running game for hot-reload
            Internal.EditorDebuggerNodeReloadScripts();

            // Hot-reload in the editor
            GodotSharpEditor.Instance.GetNode<HotReloadAssemblyWatcher>("HotReloadAssemblyWatcher").RestartTimer();

            if (Internal.IsAssembliesReloadingNeeded())
                Internal.ReloadAssemblies(softReload: false);
        }

        [UsedImplicitly]
        private void CleanSolution()
        {
            if (!File.Exists(GodotSharpDirs.ProjectSlnPath))
                return; // No solution to build

            BuildManager.BuildProjectBlocking("Debug", targets: new[] { "Clean" });
        }

        private void ViewLogToggled(bool pressed) => BuildOutputView.LogVisible = pressed;

        private void BuildMenuOptionPressed(int id)
        {
            switch ((BuildMenuOptions)id)
            {
                case BuildMenuOptions.BuildSolution:
                    BuildSolution();
                    break;
                case BuildMenuOptions.RebuildSolution:
                    RebuildSolution();
                    break;
                case BuildMenuOptions.CleanSolution:
                    CleanSolution();
                    break;
                default:
                    throw new ArgumentOutOfRangeException(nameof(id), id, "Invalid build menu option");
            }
        }

        private enum BuildMenuOptions
        {
            BuildSolution,
            RebuildSolution,
            CleanSolution
        }

        public override void _Ready()
        {
            base._Ready();

            RectMinSize = new Vector2(0, 228) * EditorScale;
            SizeFlagsVertical = (int)SizeFlags.ExpandFill;

            var toolBarHBox = new HBoxContainer { SizeFlagsHorizontal = (int)SizeFlags.ExpandFill };
            AddChild(toolBarHBox);

            buildMenuBtn = new MenuButton { Text = "Build", Icon = GetThemeIcon("Play", "EditorIcons") };
            toolBarHBox.AddChild(buildMenuBtn);

            var buildMenu = buildMenuBtn.GetPopup();
            buildMenu.AddItem("Build Solution".TTR(), (int)BuildMenuOptions.BuildSolution);
            buildMenu.AddItem("Rebuild Solution".TTR(), (int)BuildMenuOptions.RebuildSolution);
            buildMenu.AddItem("Clean Solution".TTR(), (int)BuildMenuOptions.CleanSolution);
            buildMenu.IdPressed += BuildMenuOptionPressed;

            errorsBtn = new Button
            {
                HintTooltip = "Show Errors".TTR(),
                Icon = GetThemeIcon("StatusError", "EditorIcons"),
                ExpandIcon = false,
                ToggleMode = true,
                Pressed = true,
                FocusMode = FocusModeEnum.None
            };
            errorsBtn.Toggled += ErrorsToggled;
            toolBarHBox.AddChild(errorsBtn);

            warningsBtn = new Button
            {
                HintTooltip = "Show Warnings".TTR(),
                Icon = GetThemeIcon("NodeWarning", "EditorIcons"),
                ExpandIcon = false,
                ToggleMode = true,
                Pressed = true,
                FocusMode = FocusModeEnum.None
            };
            warningsBtn.Toggled += WarningsToggled;
            toolBarHBox.AddChild(warningsBtn);

            viewLogBtn = new Button
            {
                Text = "Show Output".TTR(),
                ToggleMode = true,
                Pressed = true,
                FocusMode = FocusModeEnum.None
            };
            viewLogBtn.Toggled += ViewLogToggled;
            toolBarHBox.AddChild(viewLogBtn);

            BuildOutputView = new BuildOutputView();
            AddChild(BuildOutputView);
        }

        public override void _Notification(int what)
        {
            base._Notification(what);

            if (what == NotificationThemeChanged)
            {
                if (buildMenuBtn != null)
                    buildMenuBtn.Icon = GetThemeIcon("Play", "EditorIcons");
                if (errorsBtn != null)
                    errorsBtn.Icon = GetThemeIcon("StatusError", "EditorIcons");
                if (warningsBtn != null)
                    warningsBtn.Icon = GetThemeIcon("NodeWarning", "EditorIcons");
            }
        }
    }
}
