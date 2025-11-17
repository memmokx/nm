#include <nm/nm.h>
#include <stddef.h>

#define leftchild(r) (2 * (r) + 1)
#define rightchild(r) (2 * (r) + 2)
#define parent(r) ((r) - 1 / 2)

static int compare(const nm_symbol_t* arr, const size_t a, const size_t b, const cmp_fn cmp) {
  return cmp(&arr[a], &arr[b]);
}

static void swap(nm_symbol_t* arr, const size_t a, const size_t b) {
  const auto tmp = arr[a];
  arr[a] = arr[b];
  arr[b] = tmp;
}

void heapsort(nm_symbol_t* arr, const size_t n, const cmp_fn cmp) {
  auto start = n / 2;
  auto end = n;

  while (end > 1) {
    if (start > 0)
      start--;
    else {
      end--;
      swap(arr, end, 0);
    }

    auto root = start;
    while (leftchild(root) < end) {
      auto child = leftchild(root);
      if (child + 1 < end && compare(arr, child, child + 1, cmp) < 0)
        child++;
      if (compare(arr, root, child, cmp) < 0) {
        swap(arr, root, child);
        root = child;
      } else
        break;
    }
  }
}