#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <vector>

#include <png++/png.hpp>

using namespace std;

template<class T>
class Matrix {
public:

  Matrix() : width(0), height(0), data(0) {}

  Matrix(const size_t width, const size_t height)
    : width(width), height(height), data(new T[width * height]) {
    fill(data, data + width * height, T());
  }

  Matrix(const Matrix& that)
    : width(that.width), height(that.height) {
    data = new T[width * height];
    copy(begin(that.data), end(that.data), begin(data));
  }

  Matrix(Matrix&& that)
    : width(that.width), height(that.height), data(that.data) {
    that.clear();
  }

  ~Matrix() {
    clear();
  }

  T& operator()(const size_t x, const size_t y) {
    return data[y * width + x];
  }

  T operator()(const size_t x, const size_t y) const {
    return data[y * width + x];
  }

  size_t get_width() const { return width; }
  size_t get_height() const { return height; }

private:

  Matrix& operator=(const Matrix&) = delete;
  Matrix& operator=(Matrix&&) = delete;

  void clear() {
    width = height = 0;
    delete[] data;
  }

  size_t width;
  size_t height;

  T* data;

};

template<class T>
T next_greater_power_of_2(T n) {
  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return n + 1;
}

template<class T>
Matrix<T> make_square(const Matrix<T>& input) {
  const auto width = next_greater_power_of_2(input.get_width());
  const auto height = next_greater_power_of_2(input.get_height());
  const auto size = max(width, height);
  Matrix<T> output(size, size);
  for (size_t y = 0; y < height; ++y)
    for (size_t x = 0; x < width; ++x)
      output(x * size / width, y * size / height) = input(x, y);
  return output;
}

class QuadTree {
public:

  enum Type {
    UNDEFINED_TREE = -1,
    BLACK_TREE = 0,
    GREY_TREE = 1,
    WHITE_TREE = 2,
    SPLIT_TREE = 3,
  };

  static size_t minimum_cell_size;

  template<class T>
  QuadTree(const Matrix<T>& matrix)
    : type(UNDEFINED_TREE), parent(0) {
    init(matrix, 0, 0, matrix.get_width(), this);
  }

  size_t encoded_size() const {
    size_t result = 0;
    switch (type) {
    case UNDEFINED_TREE:
      throw runtime_error("encoded_size() on undefined tree");
    case BLACK_TREE:
    case GREY_TREE:
    case WHITE_TREE:
      result += 2;
      break;
    case SPLIT_TREE:
      for (const auto& child : children)
        if (child)
          result += child->encoded_size();
    }
    return result;
  }

  void merge_leaves() {

    if (type != SPLIT_TREE)
      return;

    for (const auto& child : children)
      child->merge_leaves();

    vector<Type> types;
    for (const auto& child : children)
      types.push_back(child->type);
    sort(begin(types), end(types));
    types.erase(unique(begin(types), end(types)), end(types));

    if (types.size() != 1 || types[0] == SPLIT_TREE)
      return;

    type = types[0];

  }

  void simplify() {

    auto leaves(get_leaves());
    size_t current_size = encoded_size();
    size_t last_size = current_size;
    size_t maximum_detail_loss = 0;

    while (!leaves.empty() && current_size > maximum_encoded_size) {

      current_size = encoded_size();

      if (last_size == current_size) {
        ++maximum_detail_loss;
        if (maximum_detail_loss > 4)
          throw runtime_error
            ("image is hopelessly complex; try a smaller cell size");
      }

      const size_t index = rand() % leaves.size();
      const auto leaf = leaves[index].lock();
      if (!merge_with_sibblings(leaf, maximum_detail_loss))
        continue;

    }

  }

private:

  QuadTree() = delete;
  QuadTree(const QuadTree&) = delete;
  QuadTree(QuadTree&&) = delete;
  QuadTree& operator=(const QuadTree&) = delete;
  QuadTree& operator=(QuadTree&&) = delete;

  template<class T>
  QuadTree(
    const Matrix<T>& matrix,
    const size_t x,
    const size_t y,
    const size_t size,
    QuadTree* const parent
  ) : parent(parent) {
    init(matrix, x, y, size, this);
  }

  template<class T>
  void init(
    const Matrix<T>& matrix,
    const size_t x,
    const size_t y,
    const size_t size,
    QuadTree* const parent
  ) {

    if (size <= minimum_cell_size) {
      const auto value = matrix(x, y);
      type = value < (255 * 1 / 5) ? BLACK_TREE
        : value < (255 * 3 / 5) ? GREY_TREE
        : WHITE_TREE;
      return;
    }

    const auto half = size / 2;
    type = SPLIT_TREE;
    children[0].reset(new QuadTree(matrix, x, y, half, parent));
    children[1].reset(new QuadTree(matrix, x + half, y, half, parent));
    children[2].reset(new QuadTree(matrix, x, y + half, half, parent));
    children[3].reset(new QuadTree(matrix, x + half, y + half, half, parent));

  }

  friend ostream& operator<<(ostream& stream, const QuadTree& tree) {
    switch (tree.type) {
    case UNDEFINED_TREE:
      return stream << "undefined";
    case BLACK_TREE:
      return stream << ".";
    case GREY_TREE:
      return stream << "/";
    case WHITE_TREE:
      return stream << "#";
    case SPLIT_TREE:
      stream << "(";
      for (const auto& child : tree.children)
        stream << *child;
      return stream << ")";
    }
  }

  Type mean_type() const {
    switch (type) {
    case UNDEFINED_TREE:
      throw runtime_error("mean_type() on undefined tree");
    case BLACK_TREE:
    case GREY_TREE:
    case WHITE_TREE:
      return type;
    case SPLIT_TREE:
      return static_cast<Type>
        ((static_cast<int>(children[0]->mean_type())
        + static_cast<int>(children[1]->mean_type())
        + static_cast<int>(children[2]->mean_type())
        + static_cast<int>(children[3]->mean_type())) / 4);
    }
  }

  bool merge_with_sibblings(
    const shared_ptr<QuadTree> tree,
    const size_t maximum_detail_loss
  ) {

    if (tree->type == SPLIT_TREE || !tree->parent)
      throw runtime_error("merge_with_sibblings() on non-leaf");

    int types[4];
    size_t sibbling_splits = 0;

    for (size_t i = 0; i < 4; ++i) {

      const auto& sibbling = tree->parent->children[i];

      if (!sibbling)
        throw runtime_error("merge_with_sibblings() with null sibbling");

      if (sibbling->type == SPLIT_TREE) {

        ++sibbling_splits;
        if (sibbling_splits > maximum_detail_loss)
          return false;

        types[i] = sibbling->mean_type();

      } else {

        types[i] = sibbling->type;

      }

    }

    const int mean = (types[0] + types[1] + types[2] + types[3]) / 4;
    if (!(mean == BLACK_TREE || mean == GREY_TREE || mean == WHITE_TREE))
      throw runtime_error("merge_with_sibblings() merged to invalid type");

    tree->parent->type = static_cast<Type>(mean);
    return true;
    
  }

  vector<weak_ptr<QuadTree>> get_leaves() {
    vector<weak_ptr<QuadTree>> result;
    get_leaves(result);
    return result;
  }

  void get_leaves(vector<weak_ptr<QuadTree>>& result) {
    for (const auto& child : children) {
      switch (child->type) {
      case UNDEFINED_TREE:
        throw runtime_error("get_leaves() on undefined tree");
      case BLACK_TREE:
      case GREY_TREE:
      case WHITE_TREE:
        result.push_back(child);
        break;
      case SPLIT_TREE:
        child->get_leaves(result);
      }
    }
  }

  static const size_t maximum_encoded_size = 903;

  Type type;
  shared_ptr<QuadTree> children[4];
  QuadTree* parent;

};

size_t QuadTree::minimum_cell_size = 64;

int main(int argc, char** argv) try {
  --argc;
  ++argv;
  if (argc < 1 || argc > 2)
    throw runtime_error("Usage: png2tweet filename.png [cell size]");

  if (argc == 2) {
    istringstream stream(argv[1]);
    if (!(stream >> QuadTree::minimum_cell_size))
      throw runtime_error("invalid cell size");
  }

  cerr << "Reading " << argv[0] << '\n';
  png::image<png::ga_pixel> image;
  try {
    image.read(argv[0], png::convert_color_space<png::ga_pixel>());
  } catch (const exception&) {}

  cerr << "Needlessly building matrix\n";
  Matrix<uint8_t> pixels(image.get_width(), image.get_height());
  for (size_t y = 0; y < image.get_height(); ++y)
    for (size_t x = 0; x < image.get_width(); ++x)
      pixels(x, y) = image.get_pixel(x, y).value;

  cerr << "Making matrix square\n";
  auto square(make_square(pixels));

  cerr << "Building quadtree\n";
  QuadTree tree(square);

  cerr << "Merging leaves\n";
  tree.merge_leaves();

  cerr << "Simplifying\n";
  tree.simplify();

  cout << tree << '\n';

} catch (const exception& error) {
  cerr << error.what() << '\n';
  return 1;
}
