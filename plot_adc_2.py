import os
import re
import matplotlib.pyplot as plt

x = []
init_step = 0
step_timer = 0.0001

current_path = os.path.dirname(__file__)
file_name = 'putty.log'

# log_file_path = open(os.path.join(current_path, './' + file_name), 'r', encoding='utf8')

test_list = []
with open(file_name, 'r', encoding='utf-8') as file:
    log_data = file.read()

pattern = r':\s*(\d+)'
matches = re.findall(pattern, log_data)

numbers = [int(match) for match in matches]

while(len(x)<len(numbers)):
    x.append(init_step)
    init_step += step_timer


print(len(x))
print(len(numbers))
plt.plot(x, numbers)
plt.show()