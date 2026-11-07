// categories.js : categories with product counts. Deleting a category that has products shows MySQL's foreign key error.
IMS.boot((content) => IMS.crud.mount(content, {
  endpoint: '/api/categories', idKey: 'category_id', nameKey: 'category_name', noun: 'category', addLabel: 'Add category',
  canWrite: true, canDelete: true, usageKey: 'product_count',
  columns: [
    { label: 'Category', key: 'category_name' },
    { label: 'Description', render: (c) => c.description || '—' },
    { label: 'Products', key: 'product_count', num: true },
  ],
  fields: [
    { key: 'category_name', label: 'Name', required: true, max: 50, wide: true },
    { key: 'description', label: 'Description', max: 150, wide: true },
  ],
}));
