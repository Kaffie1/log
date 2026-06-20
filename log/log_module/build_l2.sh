#!/bin/bash
#
# L2 ROS1 录制器编译打包脚本
# ------------------------------------------------------------
# 用途：
# 1. 以指定源码目录作为 CMake 源目录；
# 2. 清理独立的 L2 构建目录；
# 3. 打开 LOG_MODULE_BUILD_L2_ROS1_RECORDER=ON 编译 l2_ros1_recorder；
# 4. 基于 install 规则通过 checkinstall 生成 .deb 安装包。
#
# 典型使用方式：
#   ./build_l2.sh <git路径> [包名后缀] [release|debug]
#
# 说明：
# - 该脚本面向当前 log_module/L2 方案，不走 catkin_make。
# - 建议在 ROS1 Docker 容器内执行，并提前 source /opt/ros/noetic/setup.bash。

set -euo pipefail

WS_PATH="/navi_ws"
SOURCE_SUBDIR="src/log"
BUILD_SUBDIR="build/log_l2"
INSTALL_PREFIX="/opt/ros/noetic"
PKG_RELEASE="1"
PKG_REQUIRES="ros-noetic-ros-base"
PKG_OUTPUT_DIR="/navi_ws/src/dist"
ROS_DEB_NAME_PREFIX="zj-humanoid-log-l2"
PACKAGE_MODE="release"
DEFAULT_PKG_VERSION="1.0.0"
EXCLUDE_FILES="/opt/ros/noetic/.rosinstall, /opt/ros/noetic/env.sh, /opt/ros/noetic/setup.bash, /opt/ros/noetic/setup.sh, /opt/ros/noetic/setup.zsh, /opt/ros/noetic/_setup_util.py, /opt/ros/noetic/local_setup.bash, /opt/ros/noetic/local_setup.sh, /opt/ros/noetic/local_setup.zsh, /opt/ros/noetic/.catkin, /opt/ros/noetic/local_setup.fish, /tmp, /var/log"

log_info() {
    echo -e "\033[32m[INFO] $1\033[0m"
}

log_warn() {
    echo -e "\033[33m[WARN] $1\033[0m"
}

log_error() {
    echo -e "\033[31m[ERROR] $1\033[0m" >&2
    exit 1
}

get_distro_codename() {
    if command -v lsb_release >/dev/null 2>&1; then
        lsb_release -sc
        return 0
    fi

    if [ -f /etc/os-release ]; then
        . /etc/os-release
        if [ -n "${VERSION_CODENAME:-}" ]; then
            echo "${VERSION_CODENAME}"
            return 0
        fi
    fi

    log_error "无法获取系统发行版代号"
}

get_package_xml_version() {
    local repo_path=$1
    local package_version=""
    local -a package_files=()
    local -a package_versions=()
    local package_file=""
    local version=""

    if [ -f "${repo_path}/package.xml" ]; then
        package_version=$(sed -n '/<version>/ { s:.*<version>\(.*\)</version>.*:\1:p; q; }' "${repo_path}/package.xml")
    fi

    if [ -z "${package_version}" ] && [ -f "${repo_path}/ros/package.xml" ]; then
        package_version=$(sed -n '/<version>/ { s:.*<version>\(.*\)</version>.*:\1:p; q; }' "${repo_path}/ros/package.xml")
    fi

    if [ -z "${package_version}" ]; then
        mapfile -t package_files < <(find "${repo_path}" -name package.xml -type f | sort)

        for package_file in "${package_files[@]}"; do
            version=$(sed -n '/<version>/ { s:.*<version>\(.*\)</version>.*:\1:p; q; }' "${package_file}")
            if [ -n "${version}" ]; then
                package_versions+=("${version}")
            fi
        done

        if [ "${#package_versions[@]}" -eq 0 ]; then
            echo "${DEFAULT_PKG_VERSION}"
            return 0
        fi

        mapfile -t package_versions < <(printf '%s\n' "${package_versions[@]}" | sed '/^0\.0\.0$/d' | sort -u)
        if [ "${#package_versions[@]}" -eq 0 ]; then
            echo "${DEFAULT_PKG_VERSION}"
            return 0
        fi

        package_version="${package_versions[0]}"
        if [ "${#package_versions[@]}" -gt 1 ]; then
            echo "${DEFAULT_PKG_VERSION}"
            return 0
        fi
    fi

    if [ -z "${package_version}" ]; then
        echo "${DEFAULT_PKG_VERSION}"
        return 0
    fi

    echo "${package_version}"
}

normalize_deb_name() {
    local name=$1
    echo "${name}" | sed 's/[^A-Za-z0-9.+-]/-/g'
}

get_git_debug_suffix() {
    local repo_path=$1

    if [ ! -d "${repo_path}/.git" ]; then
        log_error "debug 模式需要 Git 仓库信息，但未找到 .git 目录"
    fi

    git config --global --add safe.directory "${repo_path}" >/dev/null 2>&1 || true

    local branch_name
    branch_name=$(git -C "${repo_path}" rev-parse --abbrev-ref HEAD 2>/dev/null || true)
    if [ -z "${branch_name}" ] || [ "${branch_name}" = "HEAD" ]; then
        branch_name=$(git -C "${repo_path}" describe --all --exact-match 2>/dev/null | head -1 || true)
    fi
    if [ -z "${branch_name}" ]; then
        branch_name="detached"
    fi

    local short_commit
    short_commit=$(git -C "${repo_path}" rev-parse --short HEAD 2>/dev/null || true)
    if [ -z "${short_commit}" ]; then
        log_error "debug 模式获取 commit id 失败"
    fi

    echo "$(normalize_deb_name "${branch_name}")-${short_commit}"
}

build_target_deb_name() {
    local suffix=$1
    local normalized_suffix
    normalized_suffix=$(normalize_deb_name "${suffix}")
    echo "${ROS_DEB_NAME_PREFIX}-${normalized_suffix}"
}

build_target_pkg_version() {
    local base_version=$1

    if [ "${PACKAGE_MODE}" = "debug" ]; then
        echo "${base_version}+$(get_git_debug_suffix "${GIT_ABS_PATH}")"
        return 0
    fi

    echo "${base_version}"
}

check_params() {
    if [ $# -lt 1 ]; then
        log_error "用法：$0 <git路径> [包名] [release|debug]"
    fi
}

check_env() {
    command -v cmake >/dev/null 2>&1 || log_error "未找到 cmake"
    command -v checkinstall >/dev/null 2>&1 || log_error "未找到 checkinstall"
    command -v dpkg >/dev/null 2>&1 || log_error "未找到 dpkg"
    command -v git >/dev/null 2>&1 || log_error "未找到 git"

    if [ -z "${ROS_DISTRO:-}" ]; then
        log_warn "当前未检测到 ROS_DISTRO，建议先执行：source /opt/ros/noetic/setup.bash"
    fi
}

clean_build_dir() {
    local build_path=$1
    log_info "开始清理旧编译目录：${build_path}"
    rm -rf "${build_path}"
    mkdir -p "${build_path}"
    log_info "清理完成"
}

build_ws() {
    local source_path=$1
    local build_path=$2

    log_info "开始配置 L2 构建目录"
    cmake -S "${source_path}" -B "${build_path}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DLOG_MODULE_BUILD_L2_ROS1_RECORDER=ON

    log_info "开始编译 L2 及安装所需目标"
    cmake --build "${build_path}" -j"$(nproc)"
    log_info "L2 编译完成"
}

package_ws() {
    local build_path=$1
    cd "${build_path}" || log_error "无法进入编译目录：${build_path}"

    mkdir -p "${PKG_OUTPUT_DIR}"

    sudo checkinstall -D \
        --install=no \
        --pkgname="${TARGET_PKG_NAME}" \
        --pkgversion="${TARGET_PKG_VERSION}" \
        --pkgrelease="${TARGET_PKG_RELEASE}" \
        --requires="${PKG_REQUIRES}" \
        --pakdir="${PKG_OUTPUT_DIR}" \
        --exclude="${EXCLUDE_FILES}" \
        -y \
        -- cmake --install "${build_path}"

    log_info "包名：${TARGET_PKG_NAME}，版本：${TARGET_PKG_VERSION}-${TARGET_PKG_RELEASE}"
    log_info "打包完成！安装包输出路径：${PKG_OUTPUT_DIR}"
    log_info "生成的deb包：${TARGET_PKG_NAME}_${TARGET_PKG_VERSION}-${TARGET_PKG_RELEASE}_$(dpkg --print-architecture).deb"
}

main() {
    check_params "$@"
    check_env

    local git_path_arg=$1
    local package_name=${2:-"ros1-recorder"}
    local package_mode_arg=${3:-"release"}

    cd "${git_path_arg}" || log_error "无法进入Git路径：${git_path_arg}"
    local git_abs_path
    git_abs_path=$(pwd)
    GIT_ABS_PATH="${git_abs_path}"

    local source_path="${WS_PATH}/${SOURCE_SUBDIR}"
    local build_path="${WS_PATH}/${BUILD_SUBDIR}"

    [ -d "${source_path}" ] || log_error "未找到源码目录：${source_path}"

    log_info "源码目录：${source_path}"
    log_info "构建目录：${build_path}"

    PKG_NAME=$(basename "${git_abs_path}")
    log_info "包名：${PKG_NAME}"

    PKG_VERSION=$(get_package_xml_version "${git_abs_path}")
    if [ "${PKG_VERSION}" = "${DEFAULT_PKG_VERSION}" ]; then
        log_warn "未读取到唯一有效的 package.xml 版本，使用默认版本 ${PKG_VERSION}"
    fi
    log_info "版本：${PKG_VERSION}"

    PKG_RELEASE="0$(get_distro_codename)"
    log_info "发布号：${PKG_RELEASE}"

    case "${package_mode_arg}" in
        release|debug)
            PACKAGE_MODE="${package_mode_arg}"
            ;;
        *)
            log_error "无效的打包模式：${package_mode_arg}（仅支持 release/debug）"
            ;;
    esac
    log_info "打包模式：${PACKAGE_MODE}"

    clean_build_dir "${build_path}"
    build_ws "${source_path}" "${build_path}"

    local arch
    arch=$(dpkg --print-architecture)
    TARGET_PKG_NAME=$(build_target_deb_name "${package_name}")
    TARGET_PKG_VERSION=$(build_target_pkg_version "${PKG_VERSION}")
    TARGET_PKG_RELEASE="${PKG_RELEASE}"

    package_ws "${build_path}"

    local new_deb="${PKG_OUTPUT_DIR}/${TARGET_PKG_NAME}_${TARGET_PKG_VERSION}-${TARGET_PKG_RELEASE}_${arch}.deb"
    if [ ! -f "${new_deb}" ]; then
        log_error "未找到生成的deb包：${new_deb}"
    fi

    [ -f "${build_path}/l2_ros1_recorder" ] || log_error "未找到编译产物：${build_path}/l2_ros1_recorder"

    log_info "所有操作执行完成！"
    log_info "最终打包信息："
    log_info "  包名：${TARGET_PKG_NAME}"
    log_info "  版本：${TARGET_PKG_VERSION}"
    log_info "  发布号：${TARGET_PKG_RELEASE}"
    log_info "  模式：${PACKAGE_MODE}"
    log_info "  二进制：${build_path}/l2_ros1_recorder"
    log_info "  deb：${new_deb}"
    log_info "  输出目录：${PKG_OUTPUT_DIR}"
}

main "$@"
