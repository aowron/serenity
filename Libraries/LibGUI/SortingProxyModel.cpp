/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/QuickSort.h>
#include <LibGUI/AbstractView.h>
#include <LibGUI/SortingProxyModel.h>

namespace GUI {

SortingProxyModel::SortingProxyModel(NonnullRefPtr<Model> source)
    : m_source(move(source))
    , m_key_column(-1)
{
    m_source->register_client(*this);
    invalidate();
}

SortingProxyModel::~SortingProxyModel()
{
    m_source->unregister_client(*this);
}

void SortingProxyModel::invalidate(unsigned int flags)
{
    m_mappings.clear();
    did_update(flags);
}

void SortingProxyModel::model_did_update(unsigned flags)
{
    invalidate(flags);
}

int SortingProxyModel::row_count(const ModelIndex& proxy_index) const
{
    return source().row_count(map_to_source(proxy_index));
}

int SortingProxyModel::column_count(const ModelIndex& proxy_index) const
{
    return source().column_count(map_to_source(proxy_index));
}

ModelIndex SortingProxyModel::map_to_source(const ModelIndex& proxy_index) const
{
    if (!proxy_index.is_valid())
        return {};

    ASSERT(proxy_index.model() == this);
    ASSERT(proxy_index.internal_data());

    auto& index_mapping = *static_cast<Mapping*>(proxy_index.internal_data());
    auto it = m_mappings.find(index_mapping.source_parent);
    ASSERT(it != m_mappings.end());

    auto& mapping = *it->value;
    if (static_cast<size_t>(proxy_index.row()) >= mapping.source_rows.size() || proxy_index.column() >= column_count())
        return {};
    int source_row = mapping.source_rows[proxy_index.row()];
    int source_column = proxy_index.column();
    return source().index(source_row, source_column, it->key);
}

ModelIndex SortingProxyModel::map_to_proxy(const ModelIndex& source_index) const
{
    if (!source_index.is_valid())
        return {};

    ASSERT(source_index.model() == m_source);

    auto source_parent = source_index.parent();
    auto it = const_cast<SortingProxyModel*>(this)->build_mapping(source_parent);

    auto& mapping = *it->value;

    if (source_index.row() >= static_cast<int>(mapping.proxy_rows.size()) || source_index.column() >= column_count())
        return {};

    int proxy_row = mapping.proxy_rows[source_index.row()];
    int proxy_column = source_index.column();
    if (proxy_row < 0 || proxy_column < 0)
        return {};
    return create_index(proxy_row, proxy_column, &mapping);
}

String SortingProxyModel::column_name(int column) const
{
    return source().column_name(column);
}

Variant SortingProxyModel::data(const ModelIndex& proxy_index, Role role) const
{
    return source().data(map_to_source(proxy_index), role);
}

void SortingProxyModel::update()
{
    source().update();
}

StringView SortingProxyModel::drag_data_type() const
{
    return source().drag_data_type();
}

void SortingProxyModel::set_key_column_and_sort_order(int column, SortOrder sort_order)
{
    if (column == m_key_column && sort_order == m_sort_order)
        return;

    ASSERT(column >= 0 && column < column_count());
    m_key_column = column;
    m_sort_order = sort_order;
    invalidate();
}

bool SortingProxyModel::less_than(const ModelIndex& index1, const ModelIndex& index2) const
{
    auto data1 = index1.model() ? index1.model()->data(index1, m_sort_role) : Variant();
    auto data2 = index2.model() ? index2.model()->data(index2, m_sort_role) : Variant();
    if (data1.is_string() && data2.is_string())
        return data1.as_string().to_lowercase() < data2.as_string().to_lowercase();
    return data1 < data2;
}

ModelIndex SortingProxyModel::index(int row, int column, const ModelIndex& parent) const
{
    if (row < 0 || column < 0)
        return {};

    auto source_parent = map_to_source(parent);
    const_cast<SortingProxyModel*>(this)->build_mapping(source_parent);

    auto it = m_mappings.find(source_parent);
    ASSERT(it != m_mappings.end());
    auto& mapping = *it->value;
    if (row >= static_cast<int>(mapping.source_rows.size()) || column >= column_count())
        return {};
    return create_index(row, column, &mapping);
}

ModelIndex SortingProxyModel::parent_index(const ModelIndex& proxy_index) const
{
    if (!proxy_index.is_valid())
        return {};

    ASSERT(proxy_index.model() == this);
    ASSERT(proxy_index.internal_data());

    auto& index_mapping = *static_cast<Mapping*>(proxy_index.internal_data());
    auto it = m_mappings.find(index_mapping.source_parent);
    ASSERT(it != m_mappings.end());

    return map_to_proxy(it->value->source_parent);
}

SortingProxyModel::InternalMapIterator SortingProxyModel::build_mapping(const ModelIndex& source_parent)
{
    auto it = m_mappings.find(source_parent);
    if (it != m_mappings.end())
        return it;

    auto mapping = make<Mapping>();

    mapping->source_parent = source_parent;

    int row_count = source().row_count(source_parent);
    mapping->source_rows.resize(row_count);
    mapping->proxy_rows.resize(row_count);

    for (int i = 0; i < row_count; ++i) {
        mapping->source_rows[i] = i;
    }

    // If we don't have a key column, we're not sorting.
    if (m_key_column == -1) {
        m_mappings.set(source_parent, move(mapping));
        return m_mappings.find(source_parent);
    }

    quick_sort(mapping->source_rows, [&](auto row1, auto row2) -> bool {
        bool is_less_than = less_than(source().index(row1, m_key_column, source_parent), source().index(row2, m_key_column, source_parent));
        return m_sort_order == SortOrder::Ascending ? is_less_than : !is_less_than;
    });

    for (int i = 0; i < row_count; ++i) {
        mapping->proxy_rows[mapping->source_rows[i]] = i;
    }

    if (source_parent.is_valid()) {
        auto source_grand_parent = source_parent.parent();
        build_mapping(source_grand_parent);
    }

    m_mappings.set(source_parent, move(mapping));
    return m_mappings.find(source_parent);
}

bool SortingProxyModel::is_column_sortable(int column_index) const
{
    return source().is_column_sortable(column_index);
}

}
