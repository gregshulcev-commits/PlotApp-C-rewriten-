#!/usr/bin/env bash
set -euo pipefail

APP_ID="plotapp"
APP_NAME="PlotApp"
APP_DESCRIPTION="Plotting and analysis desktop application"
WRAPPER_MARKER="# plotapp-managed-wrapper"
DESKTOP_MARKER="X-PlotApp-Managed=true"
ICON_MARKER="plotapp-managed-icon"
MANIFEST_VERSION="1"
UPDATE_STRATEGY="fresh-clone-reinstall-switch"
DEFAULT_BUILD_TYPE="Release"

SCRIPT_PATH="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)/$(basename -- "$0")"
SCRIPT_DIR="$(dirname -- "$SCRIPT_PATH")"
SOURCE_ROOT=""
if [[ -f "$SCRIPT_DIR/../CMakeLists.txt" ]]; then
    SOURCE_ROOT="$(CDPATH='' cd -- "$SCRIPT_DIR/.." && pwd -P)"
fi

log() {
    printf '[plotapp-manager] %s\n' "$*" >&2
}

warn() {
    printf '[plotapp-manager] warning: %s\n' "$*" >&2
}

die() {
    printf '[plotapp-manager] error: %s\n' "$*" >&2
    exit 1
}

has_cmd() {
    command -v "$1" >/dev/null 2>&1
}

require_cmd() {
    has_cmd "$1" || die "Required command is missing: $1"
}

xdg_data_home() {
    printf '%s\n' "${XDG_DATA_HOME:-$HOME/.local/share}"
}

xdg_cache_home() {
    printf '%s\n' "${XDG_CACHE_HOME:-$HOME/.cache}"
}

default_install_home() {
    printf '%s/%s\n' "$(xdg_data_home)" "${APP_ID}-install"
}

default_runtime_data_dir() {
    printf '%s/%s\n' "$(xdg_data_home)" "$APP_ID"
}

default_runtime_cache_dir() {
    printf '%s/%s\n' "$(xdg_cache_home)" "$APP_ID"
}

default_user_bin_dir() {
    printf '%s/.local/bin\n' "$HOME"
}

default_applications_dir() {
    printf '%s/applications\n' "$(xdg_data_home)"
}

default_icon_theme_dir() {
    printf '%s/icons/hicolor/scalable/apps\n' "$(xdg_data_home)"
}

now_utc() {
    date -u +"%Y-%m-%dT%H:%M:%SZ"
}

safe_path() {
    local path=$1
    if [[ $path = /* ]]; then
        printf '%s\n' "$path"
    else
        printf '%s/%s\n' "$PWD" "$path"
    fi
}

choose_generator() {
    if has_cmd ninja; then
        printf 'Ninja\n'
    else
        printf '%s\n' ""
    fi
}

json_escape() {
    local value=${1-}
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    value=${value//$'\n'/\\n}
    value=${value//$'\r'/\\r}
    value=${value//$'\t'/\\t}
    printf '%s' "$value"
}

manifest_escape() {
    local value=${1-}
    value=${value//$'\n'/ }
    value=${value//$'\r'/ }
    printf '%s' "$value"
}

desktop_field_escape() {
    local value=${1-}
    value=${value//$'\n'/ }
    value=${value//$'\r'/ }
    printf '%s' "$value"
}

sed_replacement_escape() {
    local value=${1-}
    value=${value//\\/\\\\}
    value=${value//&/\\&}
    value=${value//|/\\|}
    printf '%s' "$value"
}

desktop_exec_token() {
    local value
    value=$(desktop_field_escape "${1-}")
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    printf '"%s"' "$value"
}

render_desktop_template() {
    local template=$1
    local app_name=$2
    local app_comment=$3
    local exec_value=$4
    local icon_name=$5
    local safe_app_name safe_app_comment safe_exec_value safe_icon_name
    safe_app_name=$(sed_replacement_escape "$app_name")
    safe_app_comment=$(sed_replacement_escape "$app_comment")
    safe_exec_value=$(sed_replacement_escape "$exec_value")
    safe_icon_name=$(sed_replacement_escape "$icon_name")
    sed \
        -e "s|@APP_NAME@|$safe_app_name|g" \
        -e "s|@APP_COMMENT@|$safe_app_comment|g" \
        -e "s|@EXEC_PATH@|$safe_exec_value|g" \
        -e "s|@ICON_NAME@|$safe_icon_name|g" \
        "$template"
}

cleanup_install_temp_artifacts() {
    local build_dir=${1-}
    local stage_dir=${2-}
    local manifest_backup=${3-}
    local json_backup=${4-}
    [[ -n $build_dir ]] && rm -rf "$build_dir"
    [[ -n $stage_dir ]] && rm -rf "$stage_dir"
    [[ -n $manifest_backup ]] && rm -f "$manifest_backup"
    [[ -n $json_backup ]] && rm -f "$json_backup"
}

cleanup_install_state_on_exit() {
    if [[ ${PLOTAPP_INSTALL_CLEANUP_ENABLED:-0} != 1 ]]; then
        return
    fi
    cleanup_install_temp_artifacts \
        "${PLOTAPP_INSTALL_BUILD_DIR:-}" \
        "${PLOTAPP_INSTALL_STAGE_DIR:-}" \
        "${PLOTAPP_INSTALL_MANIFEST_BACKUP:-}" \
        "${PLOTAPP_INSTALL_JSON_BACKUP:-}"
}

write_file_atomic() {


    local target=$1
    local mode=$2
    local dir tmp
    dir=$(dirname -- "$target")
    mkdir -p "$dir"
    tmp=$(mktemp "$dir/.tmp.XXXXXX")
    cat > "$tmp"
    chmod "$mode" "$tmp"
    mv -f "$tmp" "$target"
}

copy_file_atomic() {
    local source=$1
    local target=$2
    local mode=$3
    local dir tmp
    [[ -f "$source" ]] || die "Cannot copy missing file: $source"
    dir=$(dirname -- "$target")
    mkdir -p "$dir"
    tmp=$(mktemp "$dir/.tmp.XXXXXX")
    cp "$source" "$tmp"
    chmod "$mode" "$tmp"
    mv -f "$tmp" "$target"
}

file_contains() {
    local path=$1
    local needle=$2
    [[ -f "$path" ]] && grep -Fqs "$needle" "$path"
}

ensure_safe_target() {
    local path=$1
    local marker=$2
    local kind=$3
    if [[ -L "$path" ]]; then
        die "$kind is a symlink and cannot be replaced safely: $path"
    fi
    if [[ -e "$path" && ! -f "$path" ]]; then
        die "$kind exists and is not a regular file: $path"
    fi
    if [[ -f "$path" ]] && ! file_contains "$path" "$marker"; then
        die "$kind exists and is not managed by $APP_NAME: $path"
    fi
}

declare -Ag MANIFEST=()

manifest_clear() {
    MANIFEST=()
}

manifest_load() {
    local path=$1 line key value
    [[ -f "$path" ]] || return 1
    manifest_clear
    while IFS= read -r line || [[ -n $line ]]; do
        [[ -z $line ]] && continue
        [[ $line == \#* ]] && continue
        [[ $line == *=* ]] || continue
        key=${line%%=*}
        value=${line#*=}
        MANIFEST["$key"]=$value
    done < "$path"
    return 0
}

manifest_get() {
    local key=$1
    local fallback=${2-}
    if [[ -v MANIFEST[$key] ]]; then
        printf '%s\n' "${MANIFEST[$key]}"
    else
        printf '%s\n' "$fallback"
    fi
}

save_previous_manifest_backup() {
    local source=$1
    local backup=$2
    if [[ -f "$source" ]]; then
        cp "$source" "$backup"
    else
        : > "$backup"
    fi
}

restore_manifest_backup() {
    local backup=$1
    local target=$2
    if [[ -s "$backup" ]]; then
        copy_file_atomic "$backup" "$target" 0644
    else
        rm -f "$target"
    fi
}

repo_default_branch() {
    local repo_url=$1
    local branch
    branch=$(git ls-remote --symref "$repo_url" HEAD 2>/dev/null | awk '/^ref:/ {sub("refs/heads/", "", $2); print $2; exit}')
    if [[ -n $branch ]]; then
        printf '%s\n' "$branch"
    else
        printf '%s\n' "main"
    fi
}

repo_branch_commit() {
    local repo_url=$1
    local branch=$2
    git ls-remote "$repo_url" "refs/heads/$branch" 2>/dev/null | awk 'NR==1 {print $1}'
}

first_line_of_file() {
    local path=$1
    if [[ -f "$path" ]]; then
        head -n 1 "$path" | tr -d '\r'
    fi
}

join_optional_flags() {
    local result=()
    if [[ $1 != off ]]; then
        result+=(gui)
    fi
    if [[ $2 == 1 ]]; then
        result+=(tests)
    fi
    local IFS=,
    printf '%s\n' "${result[*]-}"
}

probe_source_metadata() {
    local source_root=$1
    local override_repo_url=$2
    local override_branch=$3
    local override_commit=$4

    source_type=archive
    source_project_root=$(safe_path "$source_root")
    source_commit=
    source_commit_short=
    repo_url=$override_repo_url
    branch=$override_branch

    if has_cmd git && git -C "$source_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        source_type=git
        if [[ -z $repo_url ]]; then
            repo_url=$(git -C "$source_root" config --get remote.origin.url || true)
        fi
        if [[ -z $branch ]]; then
            branch=$(git -C "$source_root" symbolic-ref --quiet --short HEAD || true)
        fi
        source_commit=$(git -C "$source_root" rev-parse HEAD)
    fi

    if [[ -n $override_commit ]]; then
        source_commit=$override_commit
    fi
    if [[ -z $branch && -n $repo_url ]]; then
        branch=$(repo_default_branch "$repo_url")
    fi
    if [[ -n $source_commit ]]; then
        source_commit_short=${source_commit:0:12}
    fi
}

render_manifest_files() {
    local manifest_path=$1
    local json_path=$2

    write_file_atomic "$manifest_path" 0644 <<EOF_MANIFEST
# Managed by PlotApp desktop manager
manifest_version=$(manifest_escape "$MANIFEST_VERSION")
app_name=$(manifest_escape "$app_name")
app_id=$(manifest_escape "$app_id")
app_command=$(manifest_escape "$app_command")
cli_command=$(manifest_escape "$cli_command")
update_command=$(manifest_escape "$update_command")
uninstall_command=$(manifest_escape "$uninstall_command")
install_home=$(manifest_escape "$install_home")
current_payload_dir=$(manifest_escape "$current_payload_dir")
previous_payload_dir=$(manifest_escape "$previous_payload_dir")
system_manager_path=$(manifest_escape "$system_manager_path")
installed_at=$(manifest_escape "$installed_at")
installed_version=$(manifest_escape "$installed_version")
source_type=$(manifest_escape "$source_type")
source_project_root=$(manifest_escape "$source_project_root")
source_commit=$(manifest_escape "$source_commit")
source_commit_short=$(manifest_escape "$source_commit_short")
repo_url=$(manifest_escape "$repo_url")
branch=$(manifest_escape "$branch")
update_strategy=$(manifest_escape "$update_strategy")
build_system=$(manifest_escape "$build_system")
build_type=$(manifest_escape "$build_type")
build_generator=$(manifest_escape "$build_generator")
optional_requested=$(manifest_escape "$optional_requested")
optional_installed=$(manifest_escape "$optional_installed")
gui_requested=$(manifest_escape "$gui_requested")
gui_installed=$(manifest_escape "$gui_installed")
tests_requested=$(manifest_escape "$tests_requested")
launcher_path=$(manifest_escape "$launcher_path")
cli_launcher_path=$(manifest_escape "$cli_launcher_path")
update_launcher_path=$(manifest_escape "$update_launcher_path")
uninstall_launcher_path=$(manifest_escape "$uninstall_launcher_path")
desktop_file=$(manifest_escape "$desktop_file")
icon_path=$(manifest_escape "$icon_path")
runtime_data_dir=$(manifest_escape "$runtime_data_dir")
runtime_cache_dir=$(manifest_escape "$runtime_cache_dir")
payload_has_gui=$(manifest_escape "$payload_has_gui")
payload_has_cli=$(manifest_escape "$payload_has_cli")
EOF_MANIFEST

    write_file_atomic "$json_path" 0644 <<EOF_JSON
{
  "manifest_version": "$(json_escape "$MANIFEST_VERSION")",
  "app_name": "$(json_escape "$app_name")",
  "app_id": "$(json_escape "$app_id")",
  "app_command": "$(json_escape "$app_command")",
  "cli_command": "$(json_escape "$cli_command")",
  "update_command": "$(json_escape "$update_command")",
  "uninstall_command": "$(json_escape "$uninstall_command")",
  "install_home": "$(json_escape "$install_home")",
  "current_payload_dir": "$(json_escape "$current_payload_dir")",
  "previous_payload_dir": "$(json_escape "$previous_payload_dir")",
  "system_manager_path": "$(json_escape "$system_manager_path")",
  "installed_at": "$(json_escape "$installed_at")",
  "installed_version": "$(json_escape "$installed_version")",
  "source_type": "$(json_escape "$source_type")",
  "source_project_root": "$(json_escape "$source_project_root")",
  "source_commit": "$(json_escape "$source_commit")",
  "source_commit_short": "$(json_escape "$source_commit_short")",
  "repo_url": "$(json_escape "$repo_url")",
  "branch": "$(json_escape "$branch")",
  "update_strategy": "$(json_escape "$update_strategy")",
  "build_system": "$(json_escape "$build_system")",
  "build_type": "$(json_escape "$build_type")",
  "build_generator": "$(json_escape "$build_generator")",
  "optional_requested": "$(json_escape "$optional_requested")",
  "optional_installed": "$(json_escape "$optional_installed")",
  "gui_requested": "$(json_escape "$gui_requested")",
  "gui_installed": "$(json_escape "$gui_installed")",
  "tests_requested": "$(json_escape "$tests_requested")",
  "launcher_path": "$(json_escape "$launcher_path")",
  "cli_launcher_path": "$(json_escape "$cli_launcher_path")",
  "update_launcher_path": "$(json_escape "$update_launcher_path")",
  "uninstall_launcher_path": "$(json_escape "$uninstall_launcher_path")",
  "desktop_file": "$(json_escape "$desktop_file")",
  "icon_path": "$(json_escape "$icon_path")",
  "runtime_data_dir": "$(json_escape "$runtime_data_dir")",
  "runtime_cache_dir": "$(json_escape "$runtime_cache_dir")",
  "payload_has_gui": "$(json_escape "$payload_has_gui")",
  "payload_has_cli": "$(json_escape "$payload_has_cli")"
}
EOF_JSON
}

write_launchers() {
    local install_home=$1
    local runtime_data_dir=$2
    local runtime_cache_dir=$3
    local user_bin_dir=$4
    local launcher_path=$5
    local cli_launcher_path=$6
    local update_launcher_path=$7
    local uninstall_launcher_path=$8
    local system_manager_path=$9
    local current_payload_dir=${10}

    mkdir -p "$user_bin_dir" "$runtime_data_dir" "$runtime_cache_dir"

    ensure_safe_target "$launcher_path" "$WRAPPER_MARKER" "launcher wrapper"
    ensure_safe_target "$cli_launcher_path" "$WRAPPER_MARKER" "CLI launcher wrapper"
    ensure_safe_target "$update_launcher_path" "$WRAPPER_MARKER" "update wrapper"
    ensure_safe_target "$uninstall_launcher_path" "$WRAPPER_MARKER" "uninstall wrapper"

    write_file_atomic "$launcher_path" 0755 <<EOF_LAUNCHER
#!/usr/bin/env bash
set -euo pipefail
$WRAPPER_MARKER
CURRENT_PAYLOAD_DIR='$(manifest_escape "$current_payload_dir")'
GUI_BIN="\$CURRENT_PAYLOAD_DIR/bin/plotapp"
CLI_BIN="\$CURRENT_PAYLOAD_DIR/bin/plotapp-cli"
export PLOTAPP_RUNTIME_DATA_DIR='$(manifest_escape "$runtime_data_dir")'
export PLOTAPP_RUNTIME_CACHE_DIR='$(manifest_escape "$runtime_cache_dir")'
if [[ -x "\$GUI_BIN" ]]; then
    exec "\$GUI_BIN" "\$@"
fi
if [[ -x "\$CLI_BIN" ]]; then
    printf 'PlotApp GUI binary is not installed in %s; starting CLI instead.\n' "\$CURRENT_PAYLOAD_DIR" >&2
    exec "\$CLI_BIN" "\$@"
fi
printf 'PlotApp payload is missing under %s\n' "\$CURRENT_PAYLOAD_DIR" >&2
exit 1
EOF_LAUNCHER

    write_file_atomic "$cli_launcher_path" 0755 <<EOF_CLI
#!/usr/bin/env bash
set -euo pipefail
$WRAPPER_MARKER
CURRENT_PAYLOAD_DIR='$(manifest_escape "$current_payload_dir")'
CLI_BIN="\$CURRENT_PAYLOAD_DIR/bin/plotapp-cli"
export PLOTAPP_RUNTIME_DATA_DIR='$(manifest_escape "$runtime_data_dir")'
export PLOTAPP_RUNTIME_CACHE_DIR='$(manifest_escape "$runtime_cache_dir")'
if [[ -x "\$CLI_BIN" ]]; then
    exec "\$CLI_BIN" "\$@"
fi
printf 'PlotApp CLI binary is missing under %s\n' "\$CURRENT_PAYLOAD_DIR" >&2
exit 1
EOF_CLI

    write_file_atomic "$update_launcher_path" 0755 <<EOF_UPDATE
#!/usr/bin/env bash
set -euo pipefail
$WRAPPER_MARKER
SYSTEM_MANAGER_PATH='$(manifest_escape "$system_manager_path")'
exec "\$SYSTEM_MANAGER_PATH" update --install-home '$(manifest_escape "$install_home")' "\$@"
EOF_UPDATE

    write_file_atomic "$uninstall_launcher_path" 0755 <<EOF_UNINSTALL
#!/usr/bin/env bash
set -euo pipefail
$WRAPPER_MARKER
SYSTEM_MANAGER_PATH='$(manifest_escape "$system_manager_path")'
exec "\$SYSTEM_MANAGER_PATH" uninstall --install-home '$(manifest_escape "$install_home")' "\$@"
EOF_UNINSTALL
}

refresh_desktop_caches() {
    local applications_dir=$1
    local icon_theme_root=$2
    if has_cmd update-desktop-database; then
        update-desktop-database "$applications_dir" >/dev/null 2>&1 || true
    fi
    if has_cmd gtk-update-icon-cache; then
        gtk-update-icon-cache -q -t "$icon_theme_root" >/dev/null 2>&1 || true
    fi
}

sync_desktop_integration() {
    local current_payload_dir=$1
    local launcher_path=$2
    local desktop_file=$3
    local icon_path=$4
    local applications_dir=$5
    local icon_theme_dir=$6

    local icon_source="$current_payload_dir/share/icons/hicolor/scalable/apps/${APP_ID}.svg"
    local template_source="$current_payload_dir/share/plotapp/system/${APP_ID}.desktop.in"
    local gui_bin="$current_payload_dir/bin/plotapp"
    local icon_theme_root
    icon_theme_root=$(dirname -- "$(dirname -- "$(dirname -- "$icon_theme_dir")")")

    mkdir -p "$applications_dir" "$icon_theme_dir"

    ensure_safe_target "$icon_path" "$ICON_MARKER" "desktop icon"
    ensure_safe_target "$desktop_file" "$DESKTOP_MARKER" "desktop entry"

    if [[ -f "$icon_source" ]]; then
        copy_file_atomic "$icon_source" "$icon_path" 0644
    else
        warn "Managed install completed without icon payload: $icon_source"
    fi

    if [[ ! -x "$gui_bin" ]]; then
        rm -f "$desktop_file"
        warn "GUI binary is absent in the current payload, so no GNOME menu entry was installed"
        refresh_desktop_caches "$applications_dir" "$icon_theme_root"
        return 0
    fi

    local desktop_name desktop_comment desktop_exec
    desktop_name=$(desktop_field_escape "$APP_NAME")
    desktop_comment=$(desktop_field_escape "$APP_DESCRIPTION")
    desktop_exec="$(desktop_exec_token "$launcher_path") %F"

    if [[ -f "$template_source" ]]; then
        render_desktop_template "$template_source" "$desktop_name" "$desktop_comment" "$desktop_exec" "$APP_ID" | write_file_atomic "$desktop_file" 0644
    else
        write_file_atomic "$desktop_file" 0644 <<EOF_DESKTOP_FALLBACK
[Desktop Entry]
Type=Application
Name=$desktop_name
Comment=$desktop_comment
Exec=$desktop_exec
Icon=$APP_ID
Terminal=false
Categories=Science;Engineering;Utility;
StartupNotify=true
$DESKTOP_MARKER
EOF_DESKTOP_FALLBACK
    fi

    refresh_desktop_caches "$applications_dir" "$icon_theme_root"
}

probe_payload_capabilities() {
    local payload_dir=$1
    payload_has_gui=0
    payload_has_cli=0
    gui_installed=0
    if [[ -x "$payload_dir/bin/plotapp" ]]; then
        payload_has_gui=1
        gui_installed=1
    fi
    if [[ -x "$payload_dir/bin/plotapp-cli" ]]; then
        payload_has_cli=1
    fi
}

configure_build_and_install() {
    local source_root=$1
    local build_dir=$2
    local stage_dir=$3
    local gui_requested=$4
    local tests_requested=$5
    local build_type=$6
    local build_generator=$7
    local jobs=$8

    require_cmd cmake

    local -a configure_cmd=(cmake -S "$source_root" -B "$build_dir" "-DCMAKE_BUILD_TYPE=$build_type")
    if [[ $gui_requested == off ]]; then
        configure_cmd+=("-DPLOTAPP_BUILD_GUI=OFF")
    else
        configure_cmd+=("-DPLOTAPP_BUILD_GUI=ON")
    fi
    if [[ $tests_requested == 1 ]]; then
        configure_cmd+=("-DPLOTAPP_BUILD_TESTS=ON")
    else
        configure_cmd+=("-DPLOTAPP_BUILD_TESTS=OFF")
    fi
    if [[ -n $build_generator ]]; then
        configure_cmd+=(-G "$build_generator")
    fi

    log "Configuring build in $build_dir"
    "${configure_cmd[@]}"

    local -a build_cmd=(cmake --build "$build_dir" --parallel)
    if [[ -n $jobs ]]; then
        build_cmd+=("$jobs")
    fi
    log "Building project"
    "${build_cmd[@]}"

    if [[ $tests_requested == 1 ]]; then
        require_cmd ctest
        log "Running tests before publish"
        ctest --test-dir "$build_dir" --output-on-failure
    fi

    log "Installing payload into staging: $stage_dir"
    cmake --install "$build_dir" --prefix "$stage_dir"
}

copy_system_manager() {
    local source_manager=$1
    local target_manager=$2
    copy_file_atomic "$source_manager" "$target_manager" 0755
}

confirm_action() {
    local prompt=$1
    local yes_flag=$2
    if [[ $yes_flag == 1 ]]; then
        return 0
    fi
    if [[ ! -t 0 ]]; then
        die "$prompt Use --yes to confirm in non-interactive mode."
    fi
    local answer
    read -r -p "$prompt [y/N] " answer
    [[ $answer == [yY] || $answer == [yY][eE][sS] ]]
}

install_usage() {
    cat <<EOF_USAGE
Usage:
  install_app.sh [options]
  tools/desktop_manager.sh install [options]

Options:
  --install-home PATH       Managed install home (default: $(default_install_home))
  --runtime-data-dir PATH   Runtime user data dir (default: $(default_runtime_data_dir))
  --runtime-cache-dir PATH  Runtime cache dir (default: $(default_runtime_cache_dir))
  --user-bin-dir PATH       User launcher dir (default: $(default_user_bin_dir))
  --repo-url URL            Save GitHub/Git update source in the manifest
  --branch NAME             Branch to track for updates
  --with-gui                Require the GUI target to be built
  --without-gui             Build CLI/core payload only
  --with-tests              Run tests before publishing payload
  --without-tests           Skip tests (default)
  --build-type TYPE         CMake build type (default: $DEFAULT_BUILD_TYPE)
  --generator NAME          CMake generator (default: auto-detect Ninja)
  --jobs N                  Parallel build jobs
  --yes                     Reinstall without an interactive confirmation
EOF_USAGE
}

update_usage() {
    cat <<EOF_USAGE
Usage:
  update_app.sh [options]
  tools/desktop_manager.sh update [options]

Options:
  --install-home PATH       Managed install home (default: $(default_install_home))
  --check-only              Only compare installed and remote revisions
  --set-repo URL            Bind/update the repository used for future updates
  --branch NAME             Branch to track; if omitted, remote HEAD or main is used
  --with-gui                Override stored build preference and require GUI
  --without-gui             Override stored build preference and build without GUI
  --with-tests              Run tests before publishing the new payload
  --without-tests           Skip tests before publish
  --build-type TYPE         Override stored build type
  --generator NAME          Override stored CMake generator
  --jobs N                  Parallel build jobs
  --yes                     Skip the interactive confirmation prompt
EOF_USAGE
}

uninstall_usage() {
    cat <<EOF_USAGE
Usage:
  uninstall_app.sh [options]
  tools/desktop_manager.sh uninstall [options]

Options:
  --install-home PATH       Managed install home (default: $(default_install_home))
  --purge-data              Remove runtime data and cache directories as well
  --yes                     Skip the interactive confirmation prompt
EOF_USAGE
}

write_manifest_pair_from_values() {
    local manifest_path=$1
    local json_path=$2
    render_manifest_files "$manifest_path" "$json_path"
}

perform_install_from_source() {
    local source_root=$1
    local install_home=$2
    local runtime_data_dir=$3
    local runtime_cache_dir=$4
    local user_bin_dir=$5
    local repo_override=$6
    local branch_override=$7
    local gui_requested=$8
    local tests_requested=$9
    local build_type=${10}
    local build_generator=${11}
    local jobs=${12}
    local yes_flag=${13}

    [[ -n $source_root ]] || die "Install requires a source root"
    [[ -f "$source_root/CMakeLists.txt" ]] || die "Source root does not look like a PlotApp checkout: $source_root"

    require_cmd cmake

    local app_root="$install_home/app"
    local metadata_dir="$install_home/metadata"
    local system_dir="$install_home/system"
    local current_payload_dir="$app_root/current"
    local previous_payload_dir="$app_root/previous"
    local manifest_path="$metadata_dir/installation.manifest"
    local json_path="$metadata_dir/installation.json"
    local system_manager_path="$system_dir/desktop_manager.sh"
    local launcher_path="$user_bin_dir/$APP_ID"
    local cli_launcher_path="$user_bin_dir/${APP_ID}-cli"
    local update_launcher_path="$user_bin_dir/${APP_ID}-update"
    local uninstall_launcher_path="$user_bin_dir/${APP_ID}-uninstall"
    local applications_dir
    local icon_theme_dir
    applications_dir=$(default_applications_dir)
    icon_theme_dir=$(default_icon_theme_dir)
    local desktop_file="$applications_dir/${APP_ID}.desktop"
    local icon_path="$icon_theme_dir/${APP_ID}.svg"
    local stage_dir="$app_root/.staging-$(date +%Y%m%d%H%M%S)-$$"
    local build_dir
    build_dir=$(mktemp -d)
    local old_current_backup=""
    local manifest_backup
    local json_backup
    manifest_backup=$(mktemp)
    json_backup=$(mktemp)
    local source_manager="$source_root/tools/desktop_manager.sh"

    PLOTAPP_INSTALL_CLEANUP_ENABLED=1
    PLOTAPP_INSTALL_BUILD_DIR="$build_dir"
    PLOTAPP_INSTALL_STAGE_DIR="$stage_dir"
    PLOTAPP_INSTALL_MANIFEST_BACKUP="$manifest_backup"
    PLOTAPP_INSTALL_JSON_BACKUP="$json_backup"
    trap cleanup_install_state_on_exit EXIT

    [[ -f "$source_manager" ]] || die "Source manager is missing: $source_manager"
    mkdir -p "$app_root" "$metadata_dir" "$system_dir"
    save_previous_manifest_backup "$manifest_path" "$manifest_backup"
    save_previous_manifest_backup "$json_path" "$json_backup"

    if [[ -e "$current_payload_dir" ]]; then
        confirm_action "An installed payload already exists in $install_home. Reinstall and replace current payload?" "$yes_flag" || {
            rm -f "$manifest_backup" "$json_backup"
            rm -rf "$build_dir"
            return 1
        }
    fi

    probe_source_metadata "$source_root" "$repo_override" "$branch_override" ""

    if [[ -z $build_generator ]]; then
        build_generator=$(choose_generator)
    fi
    configure_build_and_install "$source_root" "$build_dir" "$stage_dir" "$gui_requested" "$tests_requested" "$build_type" "$build_generator" "$jobs"

    [[ -d "$stage_dir" ]] || die "Staging payload was not created: $stage_dir"
    probe_payload_capabilities "$stage_dir"
    if [[ $payload_has_cli != 1 ]]; then
        rm -rf "$build_dir" "$stage_dir"
        die "Installed payload does not contain plotapp-cli"
    fi
    if [[ $gui_requested == on && $payload_has_gui != 1 ]]; then
        rm -rf "$build_dir" "$stage_dir"
        die "GUI was explicitly requested but plotapp was not built. Install Qt6 development files or use --without-gui."
    fi

    local installed_version
    installed_version=$(first_line_of_file "$source_root/VERSION.txt")
    if [[ -z $installed_version ]]; then
        installed_version=${source_commit_short:-unknown}
    fi

    local app_name="$APP_NAME"
    local app_id="$APP_ID"
    local app_command="$APP_ID"
    local cli_command="${APP_ID}-cli"
    local update_command="${APP_ID}-update"
    local uninstall_command="${APP_ID}-uninstall"
    local installed_at
    installed_at=$(now_utc)
    local update_strategy="$UPDATE_STRATEGY"
    local build_system="cmake"
    local optional_requested
    optional_requested=$(join_optional_flags "$gui_requested" "$tests_requested")
    local optional_installed=""
    if [[ $payload_has_gui == 1 ]]; then
        optional_installed="gui"
    fi

    local gui_installed="$payload_has_gui"

    if [[ -d "$current_payload_dir" ]]; then
        old_current_backup="$app_root/.rollback-$(date +%Y%m%d%H%M%S)-$$"
        mv "$current_payload_dir" "$old_current_backup"
    fi
    if ! mv "$stage_dir" "$current_payload_dir"; then
        [[ -n $old_current_backup && -d $old_current_backup ]] && mv "$old_current_backup" "$current_payload_dir"
        rm -rf "$build_dir" "$stage_dir"
        rm -f "$manifest_backup" "$json_backup"
        die "Cannot publish staged payload into $current_payload_dir"
    fi

    if ! write_manifest_pair_from_values "$manifest_path" "$json_path"; then
        warn "Manifest publication failed, rolling back to the previous payload"
        rm -rf "$current_payload_dir"
        [[ -n $old_current_backup && -d $old_current_backup ]] && mv "$old_current_backup" "$current_payload_dir"
        restore_manifest_backup "$manifest_backup" "$manifest_path"
        restore_manifest_backup "$json_backup" "$json_path"
        rm -rf "$build_dir"
        rm -f "$manifest_backup" "$json_backup"
        die "Install aborted while writing manifest files"
    fi

    if ! write_launchers "$install_home" "$runtime_data_dir" "$runtime_cache_dir" "$user_bin_dir" "$launcher_path" "$cli_launcher_path" "$update_launcher_path" "$uninstall_launcher_path" "$system_manager_path" "$current_payload_dir"; then
        warn "Launcher publication failed, rolling back to the previous payload"
        rm -rf "$current_payload_dir"
        [[ -n $old_current_backup && -d $old_current_backup ]] && mv "$old_current_backup" "$current_payload_dir"
        restore_manifest_backup "$manifest_backup" "$manifest_path"
        restore_manifest_backup "$json_backup" "$json_path"
        rm -rf "$build_dir"
        rm -f "$manifest_backup" "$json_backup"
        die "Install aborted while writing launcher wrappers"
    fi

    if ! sync_desktop_integration "$current_payload_dir" "$launcher_path" "$desktop_file" "$icon_path" "$applications_dir" "$icon_theme_dir"; then
        warn "Desktop integration failed, rolling back to the previous payload"
        rm -rf "$current_payload_dir"
        [[ -n $old_current_backup && -d $old_current_backup ]] && mv "$old_current_backup" "$current_payload_dir"
        restore_manifest_backup "$manifest_backup" "$manifest_path"
        restore_manifest_backup "$json_backup" "$json_path"
        rm -rf "$build_dir"
        rm -f "$manifest_backup" "$json_backup"
        die "Install aborted while writing desktop integration"
    fi

    if ! copy_system_manager "$source_manager" "$system_manager_path"; then
        warn "Stable desktop manager update failed, rolling back to the previous payload"
        rm -rf "$current_payload_dir"
        [[ -n $old_current_backup && -d $old_current_backup ]] && mv "$old_current_backup" "$current_payload_dir"
        restore_manifest_backup "$manifest_backup" "$manifest_path"
        restore_manifest_backup "$json_backup" "$json_path"
        rm -rf "$build_dir"
        rm -f "$manifest_backup" "$json_backup"
        die "Install aborted while updating the stable desktop manager"
    fi

    rm -rf "$previous_payload_dir"
    if [[ -n $old_current_backup && -d $old_current_backup ]]; then
        mv "$old_current_backup" "$previous_payload_dir"
    fi

    cleanup_install_temp_artifacts "$build_dir" "$stage_dir" "$manifest_backup" "$json_backup"
    PLOTAPP_INSTALL_CLEANUP_ENABLED=0
    unset PLOTAPP_INSTALL_BUILD_DIR PLOTAPP_INSTALL_STAGE_DIR PLOTAPP_INSTALL_MANIFEST_BACKUP PLOTAPP_INSTALL_JSON_BACKUP
    trap - EXIT

    log "Managed install ready"
    log "Current payload : $current_payload_dir"
    log "User launcher   : $launcher_path"
    if [[ $payload_has_gui == 1 ]]; then
        log "GNOME desktop   : $desktop_file"
    else
        log "GNOME desktop   : skipped (GUI binary not built)"
    fi
}

print_update_status() {
    local installed_version=$1
    local installed_commit=$2
    local repo_url=$3
    local branch=$4
    local remote_commit=$5
    local status=$6
    printf 'Installed version : %s\n' "${installed_version:-unknown}"
    printf 'Installed commit  : %s\n' "${installed_commit:-unknown}"
    printf 'Update source     : %s\n' "${repo_url:-not configured}"
    printf 'Remote branch     : %s\n' "${branch:-not configured}"
    printf 'Remote commit     : %s\n' "${remote_commit:-unknown}"
    printf 'Status            : %s\n' "$status"
}

command_install() {
    local install_home
    local runtime_data_dir
    local runtime_cache_dir
    local user_bin_dir
    local repo_override=""
    local branch_override=""
    local gui_requested="auto"
    local tests_requested="0"
    local build_type="$DEFAULT_BUILD_TYPE"
    local build_generator=""
    local jobs=""
    local yes_flag=0
    local source_root="$SOURCE_ROOT"

    install_home=$(default_install_home)
    runtime_data_dir=$(default_runtime_data_dir)
    runtime_cache_dir=$(default_runtime_cache_dir)
    user_bin_dir=$(default_user_bin_dir)

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --install-home) install_home=$(safe_path "$2"); shift 2 ;;
            --runtime-data-dir) runtime_data_dir=$(safe_path "$2"); shift 2 ;;
            --runtime-cache-dir) runtime_cache_dir=$(safe_path "$2"); shift 2 ;;
            --user-bin-dir) user_bin_dir=$(safe_path "$2"); shift 2 ;;
            --repo-url) repo_override=$2; shift 2 ;;
            --branch) branch_override=$2; shift 2 ;;
            --with-gui) gui_requested=on; shift ;;
            --without-gui) gui_requested=off; shift ;;
            --with-tests) tests_requested=1; shift ;;
            --without-tests) tests_requested=0; shift ;;
            --build-type) build_type=$2; shift 2 ;;
            --generator) build_generator=$2; shift 2 ;;
            --jobs) jobs=$2; shift 2 ;;
            --source-root) source_root=$(safe_path "$2"); shift 2 ;;
            --yes) yes_flag=1; shift ;;
            --help|-h) install_usage; exit 0 ;;
            *) die "Unknown install option: $1" ;;
        esac
    done

    [[ -n $source_root ]] || die "Cannot detect source root automatically. Run install_app.sh from the project root or pass --source-root."
    perform_install_from_source "$source_root" "$install_home" "$runtime_data_dir" "$runtime_cache_dir" "$user_bin_dir" "$repo_override" "$branch_override" "$gui_requested" "$tests_requested" "$build_type" "$build_generator" "$jobs" "$yes_flag"
}

command_update() {
    local install_home
    local check_only=0
    local set_repo_url=""
    local branch_override=""
    local gui_requested_override=""
    local tests_requested_override=""
    local build_type_override=""
    local build_generator_override=""
    local jobs=""
    local yes_flag=0

    install_home=$(default_install_home)

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --install-home) install_home=$(safe_path "$2"); shift 2 ;;
            --check-only) check_only=1; shift ;;
            --set-repo|--repo-url) set_repo_url=$2; shift 2 ;;
            --branch) branch_override=$2; shift 2 ;;
            --with-gui) gui_requested_override=on; shift ;;
            --without-gui) gui_requested_override=off; shift ;;
            --with-tests) tests_requested_override=1; shift ;;
            --without-tests) tests_requested_override=0; shift ;;
            --build-type) build_type_override=$2; shift 2 ;;
            --generator) build_generator_override=$2; shift 2 ;;
            --jobs) jobs=$2; shift 2 ;;
            --yes) yes_flag=1; shift ;;
            --help|-h) update_usage; exit 0 ;;
            *) die "Unknown update option: $1" ;;
        esac
    done

    require_cmd git
    require_cmd cmake

    local manifest_path="$install_home/metadata/installation.manifest"
    local json_path="$install_home/metadata/installation.json"
    manifest_load "$manifest_path" || die "Managed installation was not found in $install_home"

    local repo_url branch installed_commit installed_version runtime_data_dir runtime_cache_dir user_bin_dir
    local gui_requested tests_requested build_type build_generator

    repo_url=$(manifest_get repo_url "")
    if [[ -n $set_repo_url ]]; then
        repo_url=$set_repo_url
    fi
    [[ -n $repo_url ]] || die "Update source is not configured. Use update_app.sh --set-repo <repo_url> [--branch <branch>] first."

    branch=$(manifest_get branch "")
    if [[ -n $branch_override ]]; then
        branch=$branch_override
    fi
    if [[ -z $branch ]]; then
        branch=$(repo_default_branch "$repo_url")
    fi

    installed_commit=$(manifest_get source_commit "")
    installed_version=$(manifest_get installed_version "unknown")
    runtime_data_dir=$(manifest_get runtime_data_dir "$(default_runtime_data_dir)")
    runtime_cache_dir=$(manifest_get runtime_cache_dir "$(default_runtime_cache_dir)")
    user_bin_dir=$(dirname -- "$(manifest_get launcher_path "$(default_user_bin_dir)/$APP_ID")")
    gui_requested=$(manifest_get gui_requested "auto")
    tests_requested=$(manifest_get tests_requested "0")
    build_type=$(manifest_get build_type "$DEFAULT_BUILD_TYPE")
    build_generator=$(manifest_get build_generator "")

    if [[ -n $gui_requested_override ]]; then
        gui_requested=$gui_requested_override
    fi
    if [[ -n $tests_requested_override ]]; then
        tests_requested=$tests_requested_override
    fi
    if [[ -n $build_type_override ]]; then
        build_type=$build_type_override
    fi
    if [[ -n $build_generator_override ]]; then
        build_generator=$build_generator_override
    fi

    if [[ -n $set_repo_url ]]; then
        source_type=$(manifest_get source_type "archive")
        source_project_root=$(manifest_get source_project_root "")
        source_commit=$(manifest_get source_commit "")
        source_commit_short=$(manifest_get source_commit_short "")
        app_name=$(manifest_get app_name "$APP_NAME")
        app_id=$(manifest_get app_id "$APP_ID")
        app_command=$(manifest_get app_command "$APP_ID")
        cli_command=$(manifest_get cli_command "${APP_ID}-cli")
        update_command=$(manifest_get update_command "${APP_ID}-update")
        uninstall_command=$(manifest_get uninstall_command "${APP_ID}-uninstall")
        install_home=$(manifest_get install_home "$install_home")
        current_payload_dir=$(manifest_get current_payload_dir "$install_home/app/current")
        previous_payload_dir=$(manifest_get previous_payload_dir "$install_home/app/previous")
        system_manager_path=$(manifest_get system_manager_path "$install_home/system/desktop_manager.sh")
        installed_at=$(manifest_get installed_at "")
        installed_version=$(manifest_get installed_version "")
        update_strategy=$(manifest_get update_strategy "$UPDATE_STRATEGY")
        build_system=$(manifest_get build_system "cmake")
        optional_requested=$(manifest_get optional_requested "")
        optional_installed=$(manifest_get optional_installed "")
        gui_installed=$(manifest_get gui_installed "0")
        launcher_path=$(manifest_get launcher_path "$user_bin_dir/$APP_ID")
        cli_launcher_path=$(manifest_get cli_launcher_path "$user_bin_dir/${APP_ID}-cli")
        update_launcher_path=$(manifest_get update_launcher_path "$user_bin_dir/${APP_ID}-update")
        uninstall_launcher_path=$(manifest_get uninstall_launcher_path "$user_bin_dir/${APP_ID}-uninstall")
        desktop_file=$(manifest_get desktop_file "$(default_applications_dir)/${APP_ID}.desktop")
        icon_path=$(manifest_get icon_path "$(default_icon_theme_dir)/${APP_ID}.svg")
        payload_has_gui=$(manifest_get payload_has_gui "0")
        payload_has_cli=$(manifest_get payload_has_cli "1")
        write_manifest_pair_from_values "$manifest_path" "$json_path"
        log "Saved update source: $repo_url [$branch]"
        if [[ $check_only != 1 && $yes_flag != 1 ]]; then
            log "Repository binding updated. Run update_app.sh --check-only or update_app.sh --yes to continue."
            exit 0
        fi
    fi

    local remote_commit
    remote_commit=$(repo_branch_commit "$repo_url" "$branch")
    [[ -n $remote_commit ]] || die "Could not resolve refs/heads/$branch from $repo_url"

    local status
    if [[ -z $installed_commit ]]; then
        status="installed commit is unknown; update is recommended"
    elif [[ $installed_commit == "$remote_commit" ]]; then
        status="up to date"
    else
        status="update available"
    fi

    if [[ $check_only == 1 ]]; then
        print_update_status "$installed_version" "$installed_commit" "$repo_url" "$branch" "$remote_commit" "$status"
        exit 0
    fi

    if [[ -n $installed_commit && $installed_commit == "$remote_commit" ]]; then
        print_update_status "$installed_version" "$installed_commit" "$repo_url" "$branch" "$remote_commit" "already up to date"
        exit 0
    fi

    print_update_status "$installed_version" "$installed_commit" "$repo_url" "$branch" "$remote_commit" "preparing managed reinstall"
    confirm_action "Fetch a fresh clone from $repo_url and replace the current managed payload?" "$yes_flag" || exit 1

    local temp_root clone_dir
    temp_root=$(mktemp -d)
    clone_dir="$temp_root/clone"
    log "Cloning $repo_url#$branch into a temporary workspace"
    git clone --depth 1 --branch "$branch" "$repo_url" "$clone_dir"
    local clone_commit
    clone_commit=$(git -C "$clone_dir" rev-parse HEAD)
    if [[ $clone_commit != "$remote_commit" ]]; then
        warn "Remote commit changed during update check; continuing with freshly cloned commit $clone_commit"
        remote_commit=$clone_commit
    fi

    perform_install_from_source "$clone_dir" "$install_home" "$runtime_data_dir" "$runtime_cache_dir" "$user_bin_dir" "$repo_url" "$branch" "$gui_requested" "$tests_requested" "$build_type" "$build_generator" "$jobs" 1
    rm -rf "$temp_root"
    log "Update completed successfully"
}

command_uninstall() {
    local install_home
    local purge_data=0
    local yes_flag=0

    install_home=$(default_install_home)

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --install-home) install_home=$(safe_path "$2"); shift 2 ;;
            --purge-data) purge_data=1; shift ;;
            --yes) yes_flag=1; shift ;;
            --help|-h) uninstall_usage; exit 0 ;;
            *) die "Unknown uninstall option: $1" ;;
        esac
    done

    local manifest_path="$install_home/metadata/installation.manifest"
    local json_path="$install_home/metadata/installation.json"
    manifest_load "$manifest_path" || die "Managed installation was not found in $install_home"

    local current_payload_dir previous_payload_dir launcher_path cli_launcher_path update_launcher_path uninstall_launcher_path desktop_file icon_path runtime_data_dir runtime_cache_dir system_manager_path
    current_payload_dir=$(manifest_get current_payload_dir "$install_home/app/current")
    previous_payload_dir=$(manifest_get previous_payload_dir "$install_home/app/previous")
    launcher_path=$(manifest_get launcher_path "$(default_user_bin_dir)/$APP_ID")
    cli_launcher_path=$(manifest_get cli_launcher_path "$(default_user_bin_dir)/${APP_ID}-cli")
    update_launcher_path=$(manifest_get update_launcher_path "$(default_user_bin_dir)/${APP_ID}-update")
    uninstall_launcher_path=$(manifest_get uninstall_launcher_path "$(default_user_bin_dir)/${APP_ID}-uninstall")
    desktop_file=$(manifest_get desktop_file "$(default_applications_dir)/${APP_ID}.desktop")
    icon_path=$(manifest_get icon_path "$(default_icon_theme_dir)/${APP_ID}.svg")
    runtime_data_dir=$(manifest_get runtime_data_dir "$(default_runtime_data_dir)")
    runtime_cache_dir=$(manifest_get runtime_cache_dir "$(default_runtime_cache_dir)")
    system_manager_path=$(manifest_get system_manager_path "$install_home/system/desktop_manager.sh")

    local prompt="Remove the managed PlotApp payload from $install_home"
    if [[ $purge_data == 1 ]]; then
        prompt+=" and purge runtime data"
    fi
    prompt+="?"
    confirm_action "$prompt" "$yes_flag" || exit 1

    rm -rf "$current_payload_dir" "$previous_payload_dir"
    rm -f "$launcher_path" "$cli_launcher_path" "$update_launcher_path" "$uninstall_launcher_path" "$desktop_file" "$icon_path" "$manifest_path" "$json_path" "$system_manager_path"
    rmdir --ignore-fail-on-non-empty "$install_home/system" "$install_home/metadata" "$install_home/app" "$install_home" 2>/dev/null || true

    if [[ $purge_data == 1 ]]; then
        rm -rf "$runtime_data_dir" "$runtime_cache_dir"
    fi

    local applications_dir
    local icon_theme_root
    applications_dir=$(default_applications_dir)
    icon_theme_root=$(dirname -- "$(dirname -- "$(dirname -- "$(default_icon_theme_dir)")")")
    refresh_desktop_caches "$applications_dir" "$icon_theme_root"
    log "Uninstall completed"
    if [[ $purge_data == 0 ]]; then
        log "Runtime data preserved in $runtime_data_dir"
        log "Runtime cache preserved in $runtime_cache_dir"
    fi
}

main() {
    [[ $# -ge 1 ]] || {
        cat <<EOF_HELP
Usage:
  $0 install [options]
  $0 update [options]
  $0 uninstall [options]

Run with --help after any subcommand to see its options.
EOF_HELP
        exit 1
    }

    local command=$1
    shift
    case "$command" in
        install) command_install "$@" ;;
        update) command_update "$@" ;;
        uninstall) command_uninstall "$@" ;;
        --help|-h|help)
            cat <<EOF_HELP
PlotApp desktop manager

Commands:
  install    Build and publish a managed payload from the current source tree
  update     Check GitHub/Git for a newer revision and reinstall from a fresh clone
  uninstall  Remove the managed payload and desktop integration
EOF_HELP
            ;;
        *) die "Unknown command: $command" ;;
    esac
}

main "$@"
