/*
 * Copyright (C) 2015 Palmer Dabbelt
 *   <palmer@dabbelt.com>
 *
 * This file is part of pconfigure.
 * 
 * pconfigure is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pconfigure is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with pconfigure.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cxx.h++"
#include "../language_list.h++"
#include "../vector_util.h++"
#include <iostream>

language_cxx* language_cxx::clone(void) const
{
    return new language_cxx();
}

bool language_cxx::can_process(const context::ptr& ctx) const
{
    if (language::all_sources_match(ctx,
                                    {std::regex(".*\\.C"),
                                     std::regex(".*\\.cxx")})) {
        return true;
    }

    /* This works around a C++11 regex bug in GCC versions prior to
     * 4.9 -- specifically, I can't match the "c++" extension because
     * the old regex implementation doesn't appear to support escaping
     * the '+' character. */
    if (language::all_sources_match(ctx, {std::regex(".*\\.c..")})) {
        for (const auto& child: ctx->children) {
            switch (child->type) {
            case context_type::DEFAULT:
            case context_type::GENERATE:
            case context_type::LIBRARY:
            case context_type::BINARY:
            case context_type::TEST:
                break;

            case context_type::SOURCE:
            {
                auto file = child->cmd->data();
                if (file.find(".c++", file.length() - 5) == std::string::npos)
                    return false;
            }
            }
        }

        return true;
    }

    return false;
}

language_cxx::shared_target language_cxx::is_shared_target(const context::ptr& ctx) const
{
    switch (ctx->type) {
        case context_type::LIBRARY:
            return shared_target::TRUE;

        case context_type::BINARY:
            return shared_target:: FALSE;

        case context_type::DEFAULT:
        case context_type::GENERATE:
        case context_type::SOURCE:
        case context_type::TEST:
            break;
    }

    std::cerr << "language_cxx: Internal error in is_shared_target()" << std::endl;
    abort();
}

std::string language_cxx::hash_link_options(const context::ptr& ctx) const
{
    return hash_options(ctx->clopts());
}

std::string language_cxx::hash_options(const std::vector<std::string>& opts) const
{
    std::hash<std::string> hash_fn;
    uint32_t seed = 5831;
    for (const auto& opt: opts)
        seed ^= hash_fn(opt) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return std::to_string(seed);
}

std::vector<makefile::target::ptr> language_cxx::targets(const context::ptr& ctx) const
{
    std::cerr << "language_c++::targets(" << std::to_string(ctx) << ")" << std::endl;

    /* We can only support some sorts of targets, it doesn't make sense to do a
     * top-level build for any others. */
    switch (ctx->type) {
        case context_type::LIBRARY:
        case context_type::BINARY:
        {
            auto objects = std::vector<target::ptr>();
            for (const auto& child: ctx->children)
                objects = objects + 

            auto link = link_objects(ctx, objects);
            return vector_util::map(objects + link,
                                    [](const target::ptr& t) -> makefile::target::ptr
                                    {
                                        return t->generate_makefile_target();
                                    });
        }

        case context_type::TEST:
        case context_type::DEFAULT:
        case context_type::GENERATE:
        case context_type::SOURCE:
            std::cerr
                << "Unable to build C++ for unsupported context type "
                << std::to_string(ctx->type)
                << "\n";
            std::cerr << std::to_string(ctx->cmd->debug()) << "\n";
            abort();
            break;
    }

    std::cerr << "language_cxx::targets(): Unknown context type\n" << std::endl;
    abort();
    return {};
}



language_cxx::link_target::link_target(const std::string& target_path, 
                                       const std::vector<target::ptr>& objects,
                                       const install_target& install,
                                       const shared_target& shared,
                                       const std::vector<std::string>& comments,
                                       const context::ptr& ctx)
: _target_path(target_path),
  _objects(objects),
  _install(install),
  _shared(shared),
  _comments(comments),
  _ctx(ctx)
{
}

makefile::target::ptr
language_cxx::link_target::generate_makefile_target(void) const
{
    auto deps = vector_util::map(_objects,
                                 [](const target::ptr& t){
                                    return t->generate_makefile_target();
                                 });
    auto target2name = [](const target::ptr& t){ return t->path(); };

    auto cmds = std::vector<std::string>{
        "mkdir -p $(dir $@)",
        std::string("$(CXX)")
          + " -o" + _target_path
          + " " + vector_util::join(vector_util::map(_objects, target2name), " ")
          + " " + vector_util::join(_ctx->link_opts, " ")
    };

    auto global = std::vector<makefile::global_targets>{
        makefile::global_targets::CLEAN,
    };

    return std::make_shared<makefile::target>(
        _target_path,
        std::string("LD++\t") + _ctx->cmd->data(),
        deps,
        global,
        cmds,
        _comments);
}

language_cxx::cp_target::cp_target(const std::string& target_path,
                                   const target::ptr& source,
                                   const install_target& install,
                                   const std::vector<std::string> comments)
: _target_path(target_path),
  _source(source),
  _install(install),
  _comments(comments)
{
}

makefile::target::ptr
language_cxx::cp_target::generate_makefile_target(void) const
{
    auto deps = vector_util::make(_source->generate_makefile_target());
    auto cmds = std::vector<std::string>{
        "mkdir -p $(dir $@)",
        "cp --reflink=auto -f " + _source->path() + " " + _target_path
    };

    auto global = [&]()
        {
            switch (_install) {
                case install_target::FALSE:
                    return std::vector<makefile::global_targets>{
                        makefile::global_targets::CLEAN,
                        makefile::global_targets::ALL,
                    };
                case install_target::TRUE:
                    return std::vector<makefile::global_targets>{
                        makefile::global_targets::UNINSTALL,
                        makefile::global_targets::INSTALL,
                    };
            }

            abort();
            return std::vector<makefile::global_targets>();
        }();


    return std::make_shared<makefile::target>(
        _target_path,
        std::string("CP ") + _target_path,
        deps,
        global,
        cmds,
        _comments);
}

std::vector<language_cxx::target::ptr>
language_cxx::link_objects(const context::ptr& ctx,
                           const std::vector<language_cxx::target::ptr>& objects)
                           const
{
    auto shared_comments = std::vector<std::string>{
        std::to_string(ctx->cmd->debug()),
        "language_cxx::link_objects()",
    };
 
    auto bin_dir = [&]() -> std::string
        {
            switch (ctx->type) {
            case context_type::BINARY:
                return ctx->bin_dir;
            case context_type::LIBRARY:
                return ctx->lib_dir;
            default: break;
            }
            
            std::cerr << "language_cxx: Internal error\n";
            abort();
            return "";
        }();
        
    auto shared_link_dir =
        ctx->obj_dir
        + "/" + bin_dir
        + "/" + ctx->cmd->data()
        + "/" + hash_link_options(ctx)
        + "/";

    /* There's actually two proper targets here: one which generates the link
     * that's targeted for installation, and one that generates the link that's
     * targeted for actual compulation. */
    auto install_target = std::make_shared<link_target>(
        shared_link_dir + "install",
        objects,
        language_cxx::install_target::TRUE,
        is_shared_target(ctx),
        shared_comments + std::vector<std::string>{"install_target"},
        ctx
    );

    auto local_target = std::make_shared<link_target>(
        shared_link_dir + "local",
        objects,
        language_cxx::install_target::FALSE,
        is_shared_target(ctx),
        shared_comments + std::vector<std::string>{"local_target"},
        ctx
    );

    /* In order to keep the local and install targets consistant with the
     * Makefile, we build the "copy" targets that depend on the generated
     * Makefile. */
    auto cp_install_target = std::make_shared<cp_target>(
        ctx->prefix + "/" + bin_dir + "/" + ctx->cmd->data(),
        install_target,
        language_cxx::install_target::TRUE,
        shared_comments + std::vector<std::string>{"cp_install_target"}
    );

     auto cp_local_target = std::make_shared<cp_target>(
        ctx->obj_dir + "/" + bin_dir + "/" + ctx->cmd->data(),
        local_target,
        language_cxx::install_target::FALSE,
        shared_comments + std::vector<std::string>{"cp_local_target"}
    );

    /* Here we return all the objects that were generated as part of this
     * linking step: that doesn't include the objects, they're expected to be
     * used elsewhere. */
    return {install_target, local_target, cp_install_target, cp_local_target};
}

static void install_cxx(void) __attribute__((constructor));
void install_cxx(void)
{
    language_list::global_add(std::make_shared<language_cxx>());
}
