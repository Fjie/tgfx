// js/models/BaseElement.ts
export default class BaseElement {
    /**
     * 创建基础图形元素
     * @param type - 元素类型（rect, circle, text）
     */
    constructor(type) {
        this.type = type;
        this.id = `el_${Date.now()}_${Math.floor(Math.random() * 1000)}`;
        this.element = document.createElementNS("http://www.w3.org/2000/svg", type);
        this.element.setAttribute('id', this.id);
        this.element.classList.add('element');
        this.element.setAttribute('filter', 'blur(5px)');
    }
    /**
     * 获取元素的属性对象
     * @returns 属性键值对
     */
    getAttributes() {
        const attrs = {};
        for (let attr of this.element.attributes) {
            attrs[attr.name] = attr.value;
        }
        return attrs;
    }
    /**
     * 设置元素的属性
     * @param name - 属性名称
     * @param value - 属性值
     */
    setAttribute(name, value) {
        this.element.setAttribute(name, value);
    }
    /**
     * 获取元素的某个属性
     * @param name - 属性名称
     * @returns 属性值
     */
    getAttribute(name) {
        return this.element.getAttribute(name);
    }
    /**
     * 设置文本内容
     * @param text - 文本内容
     */
    setText(text) {
        // 默认实现为空
    }
    /**
     * 获取文本内容
     * @returns 文本内容
     */
    getText() {
        return null;
    }
    /**
     * 渲染到指定的SVG画布
     * @param svg - 画布元素
     */
    render(svg) {
        svg.appendChild(this.element);
    }
    /**
     * 从数据创建元素
     * @param data - 元素数据
     */
    fromData(data) { }
}
//# sourceMappingURL=BaseElement.js.map