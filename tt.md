# Plan 3 — thuật toán hình học

- Cố định α = 45°, β = 35°; không lấy góc từ GY25.
- L lấy từ GY53; DX, DY cùng đơn vị với L.
- φ = atan(tan φ), tính theo a bằng công thức bên dưới, không gán cố định.
- Code đổi góc sang radian trước khi tính lượng giác; phiDeg trả về theo độ.
- Chỉ |640 − a| lấy trị tuyệt đối; giữ dấu các phần còn lại.
- Công thức chưa hiệu chuẩn: DX âm hoặc kết quả không hữu hạn được đánh dấu không hợp lệ. Plan 3 chưa có nhận diện chân vật cản nên chưa trả hướng dẫn di chuyển.

Giả sử 1 điểm T thuộc chân vật cản trên đường, trong ảnh thì ảnh của T là T_1 có tọa độ điểm ảnh là (a, b)

Ta cần xác định 2 giá trị:

1 - Khoảng cách từ T đến người đi đường theo trục Y (là trục nằm trên mặt đất, người đi đường đang tiến theo chiều dương của trục đó) là bao xa. gọi khoảng cách này là DX

2 - khoảng cách từ T theo hướng trục Y (trục vuông góc với trục X) để xác định T ở bên trái hay phải và cách lối đi của người bao xa. gọi khoảng cách này là DY.

trước tiên gọi phi là 1 góc với:

$$ tan \phi = \frac{(|640 - a|).[cos(\alpha).sin(\beta)-320.cos(\alpha - \beta)]}{320. cos(\alpha). cos(\beta)} $$

Từ 
$$ tan \phi $$

ta tính được 

$$ \phi $$

sau đó tính được

$$ DX = L. cos(\alpha). tan(\alpha + \phi) $$


$$ DY = \frac{(320 - b).L.cos(\alpha).sin(\beta).cos(\phi)}{320. cos(\alpha + \phi). cos(\beta)} $$ 
